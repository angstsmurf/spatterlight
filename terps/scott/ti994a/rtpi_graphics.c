//
//  rtpi_graphics.c
//  scott
//
//  Created by Administrator on 2026-02-12.
//
//  Loader and graphics decoder for "Return to Pirate's Isle" (RTPI),
//  a Scott Adams adventure originally released for the TI-99/4A.
//
//  The game data is distributed as a zip archive containing ROM files
//  ("c.bin" for CPU ROM, "g.bin" for GROM graphics data, or the five
//  split MAME GROM files "phm3189g3.bin" through "phm3189g7.bin").
//
//  Graphics use the TI-99/4A's VDP (Video Display Processor) tile
//  system: 256 screen positions arranged as 32 columns × 12 rows of
//  8×8-pixel tiles. Image data is stored in GROM as a bytecode stream
//  of 8 opcodes (0–7) that manipulate a 52-tile font with rotations
//  and bitwise compositing (OR/AND/XOR), followed by RLE-compressed
//  colour data. Colours use the TMS9918A's attribute format: one byte
//  per pixel row, upper nibble = foreground, lower nibble = background.
//
//  If the game data contains a standard Scott Adams header, I have not been
//  able to find it. Instead, all offsets and counts are hardcoded in DetectRTPI().
//

#include <string.h>
#include <stdlib.h>

#include "scott.h"
#include "irmak.h"
#include "detectgame.h"
#include "sagagraphics.h"
#include "unzip_in_mem.h"

#include "rtpi_graphics.h"

/* VDP screen buffer: 32 columns × 12 rows × 8 bytes per tile = 0xC00 bytes.
   vdp_pixels holds 1-bpp tile pattern data; vdp_colors holds per-row
   colour attributes (upper nibble = fg, lower nibble = bg). */
#define RTPI_IMAGE_SIZE 32 * 12 * 8

static uint8_t vdp_pixels[RTPI_IMAGE_SIZE];
static uint8_t vdp_colors[RTPI_IMAGE_SIZE];

/* Total GROM data size (5 × 0x2000 byte banks). */
#define GROM_SIZE 0xa000

#define NUMBER_OF_RTPI_ROOM_IMAGES 24 + 9
#define NUMBER_OF_RTPI_ITEM_IMAGES 7
#define NUMBER_OF_RTPI_IMAGES NUMBER_OF_RTPI_ROOM_IMAGES + NUMBER_OF_RTPI_ITEM_IMAGES
#define NUMBER_OF_RTPI_TILES 52
#define RTPI_TITLE_TEXT_LENGTH 0x1e2

/* The 52-entry tile font, loaded from the CPU ROM (c.bin). */
static uint8_t ti994atiles[NUMBER_OF_RTPI_TILES][8];

/* Debug names for the 8 draw opcodes. */
const static char *opcode_names[8] = {
    "op0_copy_tiles",
    "op1_repeat_tile",
    "op2_move_cursor",
    "op3_copy_one_tile",
    "op4_decode_and_blit_tiles",
    "op5_copy_tile_run",
    "op6_sub_opcodes",
    "op7_combine_tile_pairs"
};

/* Apply one of four rotation modes (0=none, 1=90°, 2=180°, 3=270°)
   to an 8-byte tile, using the rotation primitives from irmak.c. */
static void rotate(uint8_t *tile, int rotation) {
    switch (rotation) {
        case 0:
            break;
        case 1:
            Rot90(tile);
            break;
        case 2:
            Rot180(tile);
            break;
        case 3:
            Rot270(tile);
            break;
        default:
            break; // Should never happen
    }
}

/* Decode the RLE-compressed colour attribute stream that follows the
   tile pattern opcodes. Each byte in vdp_colors holds the colour for
   one pixel row: upper nibble = foreground, lower nibble = background.

   The encoding uses three modes:
     - Literal byte (top nibble != 0): write directly to vdp_colors.
     - Top nibble == 0x7: address jump — next byte forms a 12-bit
       write offset (in units of 8 bytes).
     - Top nibble == 0, bottom nibble != 0: repeat count in lower
       nibble; next byte is the colour to repeat (count × 8 times).
     - Byte == 0x00: next byte is a full 8-bit repeat count (for
       runs longer than 15). */
static int DecodeRTPIColors(uint8_t *ptr, uint8_t *origptr, size_t datasize) {
    uint16_t write_offset = 0;
    uint8_t *end = origptr + datasize;

    while (ptr < end && write_offset < RTPI_IMAGE_SIZE) {
        uint8_t draw_op = *ptr++;

        uint8_t top    = draw_op & 0xf0;
        uint8_t bottom = draw_op & 0x0f;

        if (top == 0x70) {
            if (ptr >= end)
                break;
            uint8_t newaddr = *ptr++;
            write_offset = ((bottom << 8) | newaddr) * 8;
            if (write_offset >= RTPI_IMAGE_SIZE)
                return 0;
            continue;
        }

        if (top != 0 && draw_op != 0) {
            vdp_colors[write_offset++] = draw_op;
            continue;
        }

        int repeat_count;
        if (draw_op == 0) {
            if (ptr >= end)
                break;
            repeat_count = *ptr++;
            if (repeat_count == 0)
                break;
        } else {
            repeat_count = bottom;
        }

        if (ptr >= end)
            break;
        int repeats = repeat_count * 8;
        uint8_t color = *ptr++;
        for (int i = 0; i < repeats; i++) {
            if (write_offset >= RTPI_IMAGE_SIZE)
                return 0;
            vdp_colors[write_offset++] = color;
        }
    }

    debug_print("DecodeRTPIColors: Finished at offset 0x%lx\n", ptr - origptr);
    return 1;
}

/* Copy a tile from the font and apply rotation. Special case: tile 0
   with rotation 1 produces a solid black (all-bits-set) tile rather
   than a rotated blank. */
static int get_tile_and_rotate(uint8_t *tile, int tile_index, int rotation) {
    if (tile_index >= NUMBER_OF_RTPI_TILES)
        return 0;
    memcpy(tile, ti994atiles[tile_index], 8);
    // Index 0 with rotation 1 means a black tile
    if (tile_index == 0 && rotation == 1) {
        memset(tile, 0xff, 8);
    } else {
        rotate(tile, rotation);
    }
    return 1;
}

/* Read one byte from the GROM stream and unpack the tile index
   (bits 0–5) and rotation (bits 6–7). */
static int get_next_byte(uint8_t **ptr, uint8_t *gend, uint16_t *tile_index, int *rotation) {
    uint8_t *gptr = *ptr;
    if (gptr >= gend)
        return 0;
    uint8_t grom_byte = *gptr++;
    *tile_index = grom_byte & 0x3f;
    *rotation = grom_byte >> 6;
    *ptr = gptr;
    return 1;
}

/* Copy 8 bytes from the VDP pixel buffer to the tile pointer.
   Bounds-checked against the buffer end. */
static int copy_tile_from_vdp(uint8_t *tile, int screen_index) {
    if (screen_index * 8 + 8 > RTPI_IMAGE_SIZE)
        return 0;
    memcpy(tile, vdp_pixels + screen_index * 8, 8);
    return 1;
}

/* Write 8 bytes of tile data to the VDP pixel buffer and advance
   the write pointer. Bounds-checked against the buffer end. */
static uint8_t *blit_tile(uint8_t *dst, uint8_t *tile) {
    if (dst + 8 > vdp_pixels + RTPI_IMAGE_SIZE)
        return dst;
    memcpy(dst, tile, 8);
    return dst + 8;
}


/* Decode a GROM image bytecode stream into vdp_pixels[] and vdp_colors[].

   When 'fuzzy' is non-zero, the stream pointer starts 2 bytes late,
   causing the bytecode to be misinterpreted — the resulting garbled
   image simulates the protagonist's myopia (used when glasses are
   not worn).

   The bytecode operates on a 16-bit instruction word per iteration:
     bits 13–15: opcode (0–7)
     bits 9–12:  repeat count / sub-opcode selector
     bits 6–7:   tile rotation (0/90/180/270)
     bits 0–5:   tile index (0–51) for tile-based ops
     bits 0–8:   screen position index for VDP-read ops

   The stream ends when opcode 7 with repeat==0 is encountered, which
   signals the transition to the colour attribute section. */
static int DrawRTPIImageWithOptionalFuzz(USImage *image, int fuzzy) {

    if (image == NULL || image->imagedata == NULL || image->datasize < 2) {
        return 0;
    }

    /* Clear colour memory for full room images to avoid residual
       colours from the previous image. Don't clear for overlay
       images (room objects) drawn on top of a base image. */
    if (image->usage == IMG_ROOM && image->index <= GameHeader.NumRooms) {
        memset(vdp_colors, 0, RTPI_IMAGE_SIZE);
    }

    uint8_t *dst = vdp_pixels;
    uint8_t *vdp_end = vdp_pixels + RTPI_IMAGE_SIZE;

    /* Fuzzy mode: skip 2 bytes to misalign the bytecode stream,
       producing a garbled image (simulates myopia). */
    uint8_t *gptr = image->imagedata + (fuzzy ? 2 : 0);
    uint8_t *gend = image->imagedata + image->datasize;

    int rotation = 0;
    int repeats = 0;
    int op = 0;

    uint16_t draw_ops_word = 0;
    uint16_t tile_index, screen_index;
    uint8_t tile[8] = {0,0,0,0,0,0,0,0};
    uint8_t worktile[8] = {0,0,0,0,0,0,0,0};

    while (gptr + 2 <= gend) {
        /*   - Bits 0-5: tile index. There are only 52 tile patterns.
         *   - Bits 6-7: rotation mode (0=none, 1=rot-90, 2=rot-180, 3=rot-270)
         *   - Some opcodes (such as 0) instead use bits 0-8 as the index to an
         *     already drawn tile in video memory (screen_index).
         *   - Bits 9-12: repeat count / subop index
         *   - Bits 13-15: opcode
         */
        draw_ops_word = READ_BE_UINT16(gptr);
        gptr += 2;
        tile_index = draw_ops_word & 0x3f;
        screen_index = draw_ops_word & 0x1ff;
        rotation = (draw_ops_word & 0xc0) >> 6;
        repeats = (draw_ops_word & 0x1e00) >> 9;
        op = (draw_ops_word & 0xe000) >> 13;

        if (repeats == 0) {
            if (op == 7) {
                return DecodeRTPIColors(gptr - 1, image->imagedata, image->datasize);
            }

            if (op != 2) {
                gptr--;
                memset(tile, 0, 8);
                if (op != 6)
                    continue;
            }
        }

        debug_print("DrawRTPIImage: tile index: %d rot:%d rep:%d op:%d\n", tile_index, rotation, repeats, op);

        if (op > 7) {
            debug_print("DrawRTPIImage: Illegal draw op: %d\n", op);
        } else {
            debug_print("DrawRTPIImage: draw op: %s\n", opcode_names[op]);
        }

        switch (op) {
            case 0:
                /* --- 0. Copy tiles: read from VDP, compose, blit, repeat ---
                 * Op 0: For each repeat, reads a tile from VDP into the buffer,
                 * blits the buffer to VDP at R4, clears the buffer, and reads
                 * the next tile index from GROM for the next iteration.
                 */
                for (int i = 0; i < repeats; i++) {
                    if (!copy_tile_from_vdp(tile, screen_index))
                        break;
                    dst = blit_tile(dst, tile);
                    if (i < repeats - 1) {
                        if (gptr >= gend)
                            break;
                        screen_index = *gptr++;
                    }
                }
                break;
            case 1:
                /* --- 1. Repeat tile. Draw tile with rotation, blit repeatedly ---
                 * Op 1: Processes a single tile with full rotation/composition,
                 * then blits the result to VDP (repeats) times. Each blit writes the
                 * same composed tile to consecutive VDP positions.
                 */
                if (!get_tile_and_rotate(tile, tile_index, rotation))
                    break;

                for (int i = 0; i < repeats; i++) {
                    dst = blit_tile(dst, tile);
                }
                memset(tile, 0, 8);
                break;
            case 2:
                /* --- 2. Move write address to tile position ---
                 * Op 2: "Move cursor" Sets the VDP write address to
                 * the pattern table location for tile (tile_index * 8),
                 * then clears the buffer. This positions the write pointer
                 * without actually writing any tile data.
                 */
                if (screen_index * 8 < RTPI_IMAGE_SIZE)
                    dst = vdp_pixels + screen_index * 8;
                break;
            case 3:
                /* --- 3. Copy one tile. Read tile from VDP and blit with repeats ---
                 * Op 3: Reads one tile from VDP into the buffer, then blits
                 * it to VDP (repeat) times.
                 */
                if (!copy_tile_from_vdp(tile, screen_index))
                    break;
                for (int i = 0; i < repeats; i++) {
                    dst = blit_tile(dst, tile);
                }
                memset(tile, 0, 8);
                break;
            case 4:
                /* --- 4. Process tile with rotation, blit, clear, repeat (0x7AD0) ---
                 * Op 4: Decode and blit tiles. For each iteration, reads a tile
                 * index from GROM, processes it with rotation, blits to VDP,
                 * clears the buffer, and repeats for (repeats) iterations.
                 */
                for (int i = 0; i < repeats; i++) {
                    if (!get_tile_and_rotate(tile, tile_index, rotation))
                        continue;
                    dst = blit_tile(dst, tile);
                    if (i < repeats - 1) {
                        if (!get_next_byte(&gptr, gend, &tile_index, &rotation))
                            break;
                    }
                }
                memset(tile, 0, 8);
                break;
            case 5:
                /* --- 5. Copy tile run from VDP ---
                 * Op 5: Reads a tile from VDP, blits it, increments the tile
                 * index, clears the buffer, and repeats. This draws a
                 * sequential run of tiles (tile N, N+1, N+2, ...).
                 */
                for (int i = 0; i < repeats; i++) {
                    if (!copy_tile_from_vdp(tile, screen_index))
                        break;
                    dst = blit_tile(dst, tile);
                    screen_index++;
                }
                memset(tile, 0, 8);
                break;
            case 6:
                /* --- 6. Lots of sub-opcodes ---
                 * Op 6: One of 16 sub-opcodes depending on
                 * the value in (repeats). Only the ones actually
                 * used by the game are implemented.
                 */

                if (repeats > 0 && repeats < 13) {
                    if (!get_tile_and_rotate(worktile, tile_index, rotation))
                        break;
                }

                switch(repeats) {
                    case 0: //  0. Writes 8 raw bytes to screen memory directly from GROM
                        if (gptr + 8 > gend || dst + 8 > vdp_end)
                            break;
                        for (int i = 0; i < 8; i++) {
                            *dst++ = *gptr++;
                        }
                        break;
                    case 1: //  1-4. OR tile_index with next (repeats) tiles, then blit and clear
                    case 2:
                    case 3:
                    case 4:
                        for (int i = 0; i < repeats; i++) {
                            for (int j = 0; j < 8; j++)
                                tile[j] |= worktile[j];
                            if (i < repeats - 1) {
                                if (!get_next_byte(&gptr, gend, &tile_index, &rotation))
                                    break;
                                if (!get_tile_and_rotate(worktile, tile_index, rotation))
                                    break;
                            }
                        }
                        dst = blit_tile(dst, tile);
                        memset(tile, 0, 8);
                        break;
                    case 6: //  6. Clear, AND tile_index with next tile, blit and clear
                        if (!get_next_byte(&gptr, gend, &tile_index, &rotation))
                            break;
                        if (!get_tile_and_rotate(tile, tile_index, rotation))
                            break;
                        for (int i = 0; i < 8; i++)
                            tile[i] &= worktile[i];
                        dst = blit_tile(dst, tile);
                        memset(tile, 0, 8);
                        break;
                    case 9: //  9. AND tile with tile_index
                        for (int i = 0; i < 8; i++) {
                            tile[i] &= worktile[i];
                        }
                        break;
                    case 10: // 10. OR tile with tile_index
                        for (int i = 0; i < 8; i++) {
                            tile[i] |= worktile[i];
                        }
                        break;
                    case 11: // 11. XOR tile with tile_index
                        for (int i = 0; i < 8; i++) {
                            tile[i] ^= worktile[i];
                        }
                        break;
                    case 13: // 13. Copy screen tile at screen_index to tile
                        copy_tile_from_vdp(tile, screen_index);
                        break;
                    case 15: // 15. Blit tile and copy tile_index to tile
                        dst = blit_tile(dst, tile);
                        if (!get_tile_and_rotate(tile, tile_index, 0))
                            memset(tile, 0, 8);
                        break;
                    default:
                        fprintf(stderr, "DrawRTPIImageWithOptionalFuzz: Unimplemented sub-opcode (%d)\n", repeats);
                        break;
                }
                break;
            case 7:
                /* --- 7. Combine tile pairs ---
                 * Op 7: OR two tiles and blit them.
                 * Repeat (repeats / 2) times.
                 */
                for (int i = 0; i < repeats; i++) {
                    if (!get_tile_and_rotate(worktile, tile_index, rotation))
                        break;
                    for (int j = 0; j < 8; j++)
                        tile[j] |= worktile[j];
                    if (i % 2 == 1) {
                        dst = blit_tile(dst, tile);
                        memset(tile, 0, 8);
                    }
                    if (i < repeats - 1) {
                        if (!get_next_byte(&gptr, gend, &tile_index, &rotation))
                            break;
                    }
                }
                break;
            default:
                fprintf(stderr, "DrawRTPIImageWithOptionalFuzz: Illegal opcode %d!\n", op);
        }
    }
    return 0;
}

/* Normal image rendering. */
int DrawRTPIImage(USImage *image) {
    return DrawRTPIImageWithOptionalFuzz(image, 0);
}

/* Render with deliberate misalignment (myopia effect). */
int DrawFuzzyRTPIImage(USImage *image) {
    return DrawRTPIImageWithOptionalFuzz(image, 1);
}

/* Set up the 16-entry palette to match the TMS9918A VDP's fixed
   colour table. RGB values are based on the MAME emulator's
   analysis of the TMS9918A's composite video output, and then
   hand adjusted. */
static void SetupRTPIColors(void) {

    /* From MAME:

     Color            Y      R-Y     B-Y     R       G       B       R   G   B
     0 Transparent
     1 Black         0.00    0.47    0.47    0.00    0.00    0.00      0   0   0
     2 Medium green  0.53    0.07    0.20    0.13    0.79    0.26     33 200  66
     3 Light green   0.67    0.17    0.27    0.37    0.86    0.47     94 220 120
     4 Dark blue     0.40    0.40    1.00    0.33    0.33    0.93     84  85 237
     5 Light blue    0.53    0.43    0.93    0.49    0.46    0.99    125 118 252
     6 Dark red      0.47    0.83    0.30    0.83    0.32    0.30    212  82  77
     7 Cyan          0.73    0.00    0.70    0.26    0.92    0.96     66 235 245
     8 Medium red    0.53    0.93    0.27    0.99    0.33    0.33    252  85  84
     9 Light red     0.67    0.93    0.27    1.13(!) 0.47    0.47    255 121 120
     A Dark yellow   0.73    0.57    0.07    0.83    0.76    0.33    212 193  84
     B Light yellow  0.80    0.57    0.17    0.90    0.81    0.50    230 206 128
     C Dark green    0.47    0.13    0.23    0.13    0.69    0.23     33 176  59
     D Magenta       0.53    0.73    0.67    0.79    0.36    0.73    201  91 186
     E Gray          0.80    0.47    0.47    0.80    0.80    0.80    204 204 204
     F White         1.00    0.47    0.47    1.00    1.00    1.00    255 255 255
     */

    glui32 black =     0x000000;
    glui32 blue =      0x5051E0;
    glui32 medgreen =  0x21c842;
    glui32 lghtgreen = 0x5edc78;
    glui32 magenta =   0xc95bba;
    glui32 darkred =   0xE15D53;
    glui32 cyan =      0x42ebf5;
    glui32 mediumred = 0xff655C;
    glui32 lightred =  0xff847e;
    glui32 yellow =    0xD4C154;
    glui32 white =     0xCDCDCD;
    glui32 brblue =    0x7F77F8;
    glui32 drkgreen =  0x00ae3f;
    glui32 bryellow =  0xEAD087;
    glui32 brwhite =   0xffffff;

    SetColor( 0, black);
    SetColor( 1, black);
    SetColor( 2, medgreen);
    SetColor( 3, lghtgreen);
    SetColor( 4, blue);
    SetColor( 5, brblue);
    SetColor( 6, darkred);
    SetColor( 7, cyan);
    SetColor( 8, mediumred);
    SetColor( 9, lightred);
    SetColor(10, yellow);
    SetColor(11, bryellow);
    SetColor(12, drkgreen);
    SetColor(13, magenta);
    SetColor(14, white);
    SetColor(15, brwhite);
}

/* Render the decoded VDP buffers (vdp_pixels + vdp_colors) to the
   screen. Iterates over each tile position, unpacking the per-row
   colour attribute (fg in upper nibble, bg in lower) and drawing
   foreground/background pixels accordingly. */
void DrawRTPIFromMem(void) {
    glk_window_clear(Graphics);

    uint8_t *ptr = vdp_pixels;
    uint8_t *colorptr = vdp_colors;
    uint8_t fg, bg;

    uint8_t *endptr = vdp_pixels + (RTPI_IMAGE_SIZE - 1);
    int x = 0; int y = 0;
    while (y < ImageHeight && ptr <= endptr) {
        for (int line = 0; line < 8; line++) {
            uint8_t byte = *ptr++;
            fg = *colorptr++;
            bg = fg & 0xf;
            fg >>= 4;
            for (int p = 0; p < 8; p++) {
                if (isNthBitSet(byte, 7 - p)) {
                    PutPixel(x + p, y + line, fg);
                } else {
                    PutPixel(x + p, y + line, bg);
                }
            }
        }
        x += 8;
        if (x > 255) {
            x = 0;
            y += 8;
        }
    }
}

/* Copy image data from GROM into a newly allocated USImage, handling
   the GROM bank header discontinuity. Each 0x2000-byte GROM bank has
   an 0x802-byte header that must be skipped when an image spans a bank
   boundary. After copying, allocates and links a new (empty) USImage
   node and returns it for the next image to use. */
static USImage *finalize_img_make_new(USImage *image, uint16_t offset, uint16_t size, uint8_t *grom) {

    if (offset >= GROM_SIZE)
        Fatal("Bad image data\n");

    image->imagedata = MemAlloc(size);
    memcpy(image->imagedata, grom + offset, size);

    uint16_t maxrange = offset + size;

    /* If the image spans a GROM bank boundary, the middle portion
       contains a bank header (0x802 bytes) that is not image data.
       Re-copy the tail from past the header to stitch the image
       back together. */
    if (maxrange > 0x17fe && (maxrange - 0x17fe) % 0x2000 < size) {
        uint16_t cutoff = size - ((maxrange - 0x17fe) % 0x2000);
        if (offset + cutoff + 0x802 + size - cutoff >= GROM_SIZE)
            Fatal("Bad image data\n");
        memcpy(image->imagedata + cutoff, grom + offset + cutoff + 0x802, size - cutoff);
    }

    image->datasize = size;

    image->next = NewImage();
    image->next->previous = image;
    image = image->next;
    return image;
}

#define LAST_IMAGE_SIZE 0x72

/* Build a linked list of USImage structs from the GROM data. Room
   images come first (24 rooms + 9 extra scenes), followed by 7 item
   overlay images. Image sizes are generally computed from the gap
   between consecutive offsets, with hardcoded overrides for images
   whose data is not contiguous (17, 22, 26). */
static int LoadRTPIGraphics(uint16_t *room_image_offsets, uint16_t item_image_offsets[NUMBER_OF_RTPI_ITEM_IMAGES][2], uint8_t *grom) {
    USImages = NewImage();
    USImage *image = USImages;

    for (int i = 0; i < NUMBER_OF_RTPI_ROOM_IMAGES; i++) {

        if (room_image_offsets[i] == 0)
            continue;

        size_t offset = room_image_offsets[i];

        image->systype = SYS_TI994A;
        image->usage = IMG_ROOM;
        image->index = i;
        if (i == 25) {
            /* Image 25 is the Texas Instruments logo / title screen;
               map it to the standard title image index (99). */
            image->index = 99;
        } else if (i == 27) {
            /* Smuggler's hold from below — displayed as an object
               overlay for room 29 (inside of boat). */
            image->index = 29;
            image->usage = IMG_ROOM_OBJ;
        } else if (i == 28) {
            /* Deck at night — displayed as an object overlay for
               room 4 (isle off in distance). */
            image->index = 4;
            image->usage = IMG_ROOM_OBJ;
        }

        size_t size;

        /* Some images span GROM bank headers, so their sizes can't be computed
         from the offset table and must be hardcoded. */
        if (i == 17) {
            size = 0x183;
        } else if (i == 22) {
            size = 0xf7;
        } else if (i == 26) {
            size = 0xdd4;
        } else if (i < NUMBER_OF_RTPI_ROOM_IMAGES - 1) {
            /* Normal case: size = next non-overlapping offset minus ours. */
            int j = i + 1;
            size_t next;
            do {
                next = room_image_offsets[j++];
            } while (next < offset);
            size = next - offset;
        } else {
            /* Last room image: size extends to the first item image. */
            size = item_image_offsets[0][1] - offset;
        }

        image = finalize_img_make_new(image, offset, size, grom);
    }

    /* Item overlay images. Each entry in item_image_offsets[][] is a pair:
       [0] = index of room image the item overlays, [1] = GROM offset of image data.
       Size is computed from the gap to the next item image, except for the
       last one which has a hardcoded size (LAST_IMAGE_SIZE). */
    for (int i = 0; i < NUMBER_OF_RTPI_ITEM_IMAGES; i++) {
        size_t offset = item_image_offsets[i][1];

        size_t size;
        if (i < NUMBER_OF_RTPI_ITEM_IMAGES - 1) {
            size = item_image_offsets[i + 1][1] - offset;
        } else {
            size = LAST_IMAGE_SIZE;
        }

        image->systype = SYS_TI994A;
        image->usage = IMG_ROOM_OBJ;
        image->index = item_image_offsets[i][0];

        image = finalize_img_make_new(image, offset, size, grom);
    }

    /* The loop always leaves one empty trailing USImage node.
       Unlink and free it so the list ends cleanly. */
    if (image->imagedata == NULL) {
        if (image != USImages) {
            image->previous->next = NULL;
            free(image);
            image = NULL;
        } else {
            free(USImages);
            USImages = NULL;
        }
    }

    return 1;

    /*
     Image for room 0 (.) has offset 0x8000
     Image for room 1 (bottom bunk) has offset 0x8047
     Image for room 2 (ship's cabin) has offset 0x831f
     Image for room 3 (*I'm on a dock) has offset 0x854c
     Image for room 4 (*I'm on deck) has offset 0x87d7
     Image for room 5 (beach by a small hill) has offset 0x8953
     Image for room 6 (*I'm on ledge 8 feet below hill summit) has offset 0x8ab2
     Image for room 7 (*I'm on top of a small hill) has offset 0x8d10
     Image for room 8 (cavern) has offset 0x0000
     Image for room 9 (tool shed) has offset 0x0000
     Image for room 10 (*Sorry, to explore Pirate's Isle you'll need Adventure #2) has offset 0x8e84
     Image for room 11 (sea) has offset 0x8fcf
     Image for room 12 (*I'm undersea) has offset 0x90c8
     Image for room 13 (smugglers hold inside ship) has offset 0x92fe
     Image for room 14 (hall) has offset 0x0000
     Image for room 15 (*I'm on a rocky beach by sea) has offset 0x9426
     Image for room 16 (top bunk) has offset 0x9619
     Image for room 17 (*I'm on the engine) has offset 0x9773
     Image for room 18 (*I'm in an engine room) has offset 0xa0fa
     Image for room 19 (*I'm on catwalk on outside of ship by porthole) has offset 0xa378
     Image for room 20 (narrow crawlway) has offset 0xa468
     Image for room 21 (ship's hold) has offset 0xa54e
     Image for room 22 (*I'm on beam under dock) has offset 0x8fcf
     Image for room 23 (sunken ship) has offset 0xa5ca
     Image for room 24 (Never Never Land) has offset 0xa86a
     Image for room 25 () has offset 0xab0f // Title screen / Texas Instruments logo
     Image for room 26 () has offset 0xadc2 // Congrats, victory screen
     Image for room 27 () has offset 0xc39a // Smuggler's hold from below
     Image for room 28 () has offset 0xc47c // Boat's deck at night with island in the distance
     Image for room 29 () has offset 0xc588 // Sea at night
     Image for room 30 () has offset 0xc610 // High beam (to be drawn over Underside of Dock)
     Image for room 31 () has offset 0xc676 // Low beam (to be drawn over I'm on beam under dock)
     Image for room 32 () has offset 0xc6dc // Boat to the west at night (to be drawn over 29)
     Image for room object 63 (Sunken Ship) has offset 0xc775
     Image for room object 26 (Dock to the East) has offset 0xc8f7
     Image for room object 28 (Boat to the WEST) has offset 0xc9f1
     Image for room object 36 (Rocky beach) has offset 0xcaa2
     Image for room object 27 (Underside of Dock) has offset 0xcb04
     Image for room object 7  (Sleeping Pirate) has offset 0xcca6
     Image for room object 52 (Pirate at helm) has offset 0xcd08
     */
}

/* Forward declarations for shared Scott Adams loader functions
   defined in saga.c. */
GameIDType FreeGameResources(void);
uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof);
uint8_t *ParseRooms(uint8_t *ptr, uint8_t *endptr, int number_of_rooms);
uint8_t *ParseItems(uint8_t *ptr, uint8_t *endptr, int number_of_items);

/* Parse the dictionary from the GROM data stream into the global
   Nouns[] and Verbs[] arrays.

   The dictionary is a flat byte stream of fixed-width words
   (info.word_length chars each). Nouns come first, then verbs. A '*'
   character marks a synonym: it restarts the current word entry so
   the synonym shares the same word number (the '*' prefix is preserved
   in the stored string, as the game engine uses it to identify synonyms).
   Trailing spaces within a word are collapsed. A byte with bit 7 set
   signals the end of the dictionary.

   When `loud` is set, each parsed word is printed for debugging. */
static uint8_t *ReadRTPIDictionary(GameInfo info, uint8_t *ptr, uint8_t *endptr, int loud) {
    int wl = info.word_length;
    if (wl < 1 || wl > 1020)
        Fatal("Bad word length");

    #define DICTWORD_SIZE 1024
    char dictword[DICTWORD_SIZE];
    char c = 0;
    int wordnum = 0;
    int charindex = 0;

    int nw = info.number_of_words;
    int nv = info.number_of_verbs;
    int nn = info.number_of_nouns;

    /* Initialize all slots to "." (the Scott Adams placeholder for
       empty/unused dictionary entries). */
    for (int i = 0; i < nw + 2; i++) {
        Verbs[i] = ".";
        Nouns[i] = ".";
    }

    do {
        int restarts = 0;
        for (int i = 0; i < wl; i++) {
            if (ptr >= endptr)
                return ptr;
            c = *ptr++;

            /* Skip leading NUL bytes (padding between entries). */
            if (c == 0 && charindex == 0) {
                if (ptr >= endptr)
                    return ptr;
                c = *ptr++;
            }
            /* Collapse trailing spaces: if the previous char was a space
               and the current one isn't, back up to overwrite the space. */
            if (c != ' ' && charindex > 0 && dictword[charindex - 1] == ' ') {
                i--;
                charindex--;
            }
            /* '*' marks a synonym — reset the word buffer and restart
               reading so this entry replaces the previous one at the
               same word number. The '*' prefix is preserved. */
            if (c == '*') {
                charindex = 0;
                i = -1;
                if (++restarts > wl)
                    break;
            }
            if (charindex < DICTWORD_SIZE - 1)
                dictword[charindex++] = c;
        }
        dictword[charindex] = 0;
        if (wordnum < nn) {
            Nouns[wordnum] = MemAlloc(charindex + 1);
            memcpy((char *)Nouns[wordnum], dictword, charindex + 1);
            if (loud)
                debug_print("Noun %d: \"%s\"\n", wordnum, Nouns[wordnum]);
        } else if (wordnum - nn < nw + 2) {
            Verbs[wordnum - nn] = MemAlloc(charindex + 1);
            memcpy((char *)Verbs[wordnum - nn], dictword, charindex + 1);
            if (loud)
                debug_print("Verb %d: \"%s\"\n", wordnum - nn, Verbs[wordnum - nn]);
        }
        wordnum++;

        /* A byte with the high bit set terminates the dictionary. */
        if (c > 127)
            return ptr;

        charindex = 0;
    } while (wordnum <= nv + nn);

    return ptr;
    #undef DICTWORD_SIZE
}

/* Parse the action table from the GROM data.

   Unlike the ASCII Scott Adams .dat format, where each action is stored
   as a contiguous record, the binary format uses a column-major
   layout: all verbs for every action come first (one byte per action),
   then all nouns, then subcommand pairs, and finally conditions. Each
   "column" is (number_of_actions + 1) entries long.

   Layout within `data` starting at `offset`:
     Column 0: verb[0..N]                    — 1 byte each
     Column 1: noun[0..N]                    — 1 byte each
     Columns 2–5: subcommand values (4 cols) — 1 byte each, paired into 2 subcommands
     Columns 6–15: conditions (5 cols)       — 2 bytes each (big-endian)

   Vocab is encoded as verb*150 + noun, and subcommands as value*150 + value2,
   matching the standard Scott Adams action encoding. Condition offsets
   that cross the 0x97FE boundary wrap around (GROM bank boundary). */
static int ParseRTPIActions(uint8_t *ptr, uint8_t *data, size_t datalength, int number_of_actions)
{
    Action *ap = Actions;
    size_t base = ptr - data;
    size_t stride = (size_t)(number_of_actions + 1);

    /* Pre-compute the base offset of each single-byte column (0–5)
       and the base offset of the conditions block (columns 6–15,
       which are 16-bit). */
    size_t col[6];
    for (int c = 0; c < 6; c++)
        col[c] = base + c * stride;
    size_t cond_base = base + 6 * stride;

    /* The farthest single-byte read is col[5] + number_of_actions.
       Bail out early if that's already past the data. */
    if (col[5] + number_of_actions >= datalength)
        return 0;

    for (int i = 0; i <= number_of_actions; i++) {
        /* Read verb and noun from their respective columns. */
        ap->Vocab = data[col[0] + i] * 150 + data[col[1] + i];

        /* Read 2 subcommands, each built from a pair of consecutive columns. */
        ap->Subcommand[0] = 150 * data[col[2] + i] + data[col[3] + i];
        ap->Subcommand[1] = 150 * data[col[4] + i] + data[col[5] + i];

        /* Conditions are 16-bit big-endian values in 5 columns following
           the 6 single-byte columns. Each condition column has a stride
           of 2 * stride bytes. */
        for (int j = 0; j < 5; j++) {
            size_t cond_off = cond_base + (size_t)i * 2 + (size_t)j * stride * 2;
            /* Wrap around GROM bank boundary at 0x97FE. */
            if (cond_off >= 0x97fe)
                cond_off -= 0x97fe;
            if (cond_off + 2 > datalength) {
                ap->Condition[j] = 0;
                continue;
            }
            ap->Condition[j] = READ_BE_UINT16(data + cond_off);
        }

        debug_print("Action %d Vocab: %d (Verb:%d/NounOrChance:%d)\n", i, ap->Vocab,
                    ap->Vocab / 150,  ap->Vocab % 150);
        debug_print("Action %d Condition[0]: %d (%d/%d)\n", i,
                    ap->Condition[0], ap->Condition[0] % 20, ap->Condition[0] / 20);
        debug_print("Action %d Condition[1]: %d (%d/%d)\n", i,
                    ap->Condition[1], ap->Condition[1] % 20, ap->Condition[1] / 20);
        debug_print("Action %d Condition[2]: %d (%d/%d)\n", i,
                    ap->Condition[2], ap->Condition[2] % 20, ap->Condition[2] / 20);
        debug_print("Action %d Condition[3]: %d (%d/%d)\n", i,
                    ap->Condition[3], ap->Condition[3] % 20, ap->Condition[3] / 20);
        debug_print("Action %d Condition[4]: %d (%d/%d)\n", i,
                    ap->Condition[4], ap->Condition[4] % 20, ap->Condition[4] / 20);
        debug_print("Action %d Subcommand [0]]: %d (%d/%d)\n", i, ap->Subcommand[0], ap->Subcommand[0] % 150, ap->Subcommand[0] / 150);
        debug_print("Action %d Subcommand [1]]: %d (%d/%d)\n", i, ap->Subcommand[1], ap->Subcommand[1] % 150, ap->Subcommand[1] / 150);

        ap++;
    }

    /* Chack if we ran off the end of data. */
    size_t end_off = cond_base + (size_t)number_of_actions * 2 + 4 * stride * 2 + 2;
    return (end_off <= datalength);
}

/* Parse room exit connections from a column-major byte table.

   Like the action table, this is stored column-major: all North exits
   for every room come first, then all South exits, etc. There are 6
   directions (N/S/E/W/U/D) and (number_of_rooms + 1) entries per
   direction. Each byte is the destination room number (0 = no exit).

   Room images are initialised to 255 (no image) here; the actual
   image assignment happens later during gameplay. */
static uint8_t *ParseRTPIRoomConnections(uint8_t *ptr, uint8_t *endptr, int number_of_rooms) {
    for (int j = 0; j < 6; j++) {
        int counter = 0;
        Room *rp = Rooms;

        while (counter <= number_of_rooms && ptr < endptr) {
            rp->Image = 255;
            rp->Exits[j] = *ptr;
            debug_print("Room %d (%s) exit %d (%s): %d\n", counter, Rooms[counter].Text, j, Nouns[j + 1], rp->Exits[j]);
            ptr++;
            counter++;
            rp++;
        }
    }
    return ptr;
}


/* Load all game data (dictionary, rooms, messages, items, actions,
   room connections) from the GROM data for Return to Pirate's Isle.

   The data is at absolute offsets within the GROM image (specified in
   the GameInfo struct). Each section is parsed sequentially, with
   bounds checks between sections. Returns the game ID on success, or
   UNKNOWN_GAME if any section extends past the data boundary. */
static GameIDType LoadRTPI(uint8_t *data, size_t length, GameInfo info, int loud)
{
    uint8_t *ptr;
    uint8_t *endptr = data + length;

    int ct;

#pragma mark dictionary

    ptr = data + info.start_of_dictionary;
    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ReadRTPIDictionary(info, ptr, endptr, loud);

#pragma mark rooms

    ptr = data + info.start_of_room_descriptions;
    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseRooms(ptr - 1, endptr, GameHeader.NumRooms);

#pragma mark messages

    ptr = data + info.start_of_messages;
    if (ptr >= endptr)
        return FreeGameResources();

    ct = 0;

    /* Messages are length-prefixed strings (1-byte length, then that
       many characters). Empty messages (length 0) are stored as ".". */
    while (ct < GameHeader.NumMessages + 1) {
        if (ptr >= endptr)
            break;
        uint8_t stringlength = *ptr++;
        if (stringlength != 0) {
            Messages[ct] = MemAlloc(stringlength + 1);
            /* Message 60 spans a GROM bank boundary — the middle
               0x802 bytes are a bank header, not message data. Stitch
               the two halves together, skipping the header. */
            if (ct == 60) {
                if (ptr + 18 > endptr || ptr + 0x814 + 7 > endptr) {
                    Messages[ct][0] = 0;
                } else {
                    memcpy(Messages[ct], ptr, 18);
                    memcpy((uint8_t *)Messages[ct] + 18, ptr + 0x814, 7);
                }
                ptr += 0x802;
            } else {
                if (ptr + stringlength <= endptr)
                    memcpy(Messages[ct], ptr, stringlength);
            }
            Messages[ct][stringlength] = 0;
            if (loud)
                debug_print("Message %d: \"%s\"\n", ct, Messages[ct]);
            ptr += stringlength;
        } else {
            Messages[ct] = ".";
        }
        ct++;
    }

#pragma mark items

    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseItems(ptr, endptr, GameHeader.NumItems);

#pragma mark item locations

    ptr = data + info.start_of_item_locations;
    if (ptr >= endptr)
        return FreeGameResources();

    /* Item locations are a flat array of bytes, one per item. Each
       byte is the room number where the item starts. */
    ct = 0;
    Item *ip = Items;
    while (ct <= GameHeader.NumItems && ptr < endptr) {
        ip->Location = *ptr++;
        ip->InitialLoc = ip->Location;
        if (Items[ct].Text && ip->Location < GameHeader.NumRooms && Rooms[ip->Location].Text)
            debug_print("Location of item %d, \"%s\":%d\n", ct, Items[ct].Text, ip->Location);
        ip++;
        ct++;
    }

    ptr = data + info.start_of_actions;
    if (ptr >= endptr)
        return FreeGameResources();

    ;
    if (!ParseRTPIActions(ptr, data, length, GameHeader.NumActions)) {
        return FreeGameResources();
    }

    ptr = data + info.start_of_room_connections;
    if (ptr >= endptr)
        return FreeGameResources();

    ptr = ParseRTPIRoomConnections(ptr, endptr, GameHeader.NumRooms);

    return info.gameID;
}

/* Translate a TI-99/4A GROM address into a file offset.

   In the original hardware, GROM addresses start at 0x6000. The file
   image starts at offset 0, so we subtract 0x6000 from any address
   that has bit 15 set (i.e. addresses >= 0x8000, which are in the
   range the game uses). Addresses below 0x8000 are left unchanged. */
static uint16_t adjust_grom_offset(uint16_t offset) {
    if ((offset & 0x8000) != 0) {
        offset -= 0x6000;
    }
    return offset;
}

/* Byte offsets within the c.bin (CPU ROM) file where the image offset
   tables begin. ROOM_IMAGES_OFFSET points to 33 big-endian 16-bit
   GROM addresses (one per room image). ITEM_IMAGES_OFFSET points to
   7 pairs of (room_index, GROM_address), each 4 bytes. */
#define ROOM_IMAGES_OFFSET 0x12
#define ITEM_IMAGES_OFFSET 0x54

/* Detect and load the TI-99/4A "Return to Pirate's Isle" game from
   a zip archive.

   The game ships as a set of ROM files inside a zip:
     - c.bin (or phm3189c.bin): CPU ROM — contains tile font data,
       image offset tables, and the title screen text
     - g.bin (or phm3189g3–g7.bin): GROM data — contains all game data
       (dictionary, rooms, messages, items, actions, connections) and
       the image data

   Two zip layouts are supported: the "clean" layout with c.bin + g.bin,
   and the MAME layout with the split phm3189*.bin files.

   On success, all game data structures are populated and the function
   returns RETURN_TO_PIRATES_ISLE. On failure (missing files, bad data)
   it returns UNKNOWN_GAME. */
GameIDType DetectRTPI(uint8_t *data, size_t datalength) {

    size_t rom_length;

    /* Try to extract the CPU ROM, accepting either filename variant. */
    uint8_t *cpu_rom = extract_file_from_zip_data(data, datalength, "c.bin", &rom_length);

    /* If c.bin is not present, try the MAME zip name */
    if (!cpu_rom) {
        cpu_rom = extract_file_from_zip_data(data, datalength, "phm3189c.bin", &rom_length);
        if (!cpu_rom) {
            return UNKNOWN_GAME;
        }
    }

    if (rom_length < 0x1e94) {
        free(cpu_rom);
        return UNKNOWN_GAME;
    }

    /* Extract room and item image offset tables from the CPU ROM.
       Room offsets are 33 big-endian 16-bit GROM addresses at 0x12.
       Item offsets are 7 (room_index, GROM_address) pairs at 0x54. */
    uint16_t room_image_offsets[NUMBER_OF_RTPI_ROOM_IMAGES];

    for (int i = 0; i < NUMBER_OF_RTPI_ROOM_IMAGES; i++) {
        room_image_offsets[i] = adjust_grom_offset(READ_BE_UINT16(cpu_rom + ROOM_IMAGES_OFFSET + i * 2));
    }

    uint16_t item_image_offsets[NUMBER_OF_RTPI_ITEM_IMAGES][2];

    for (int i = 0; i < 7; i++) {
        item_image_offsets[i][0] = READ_BE_UINT16(cpu_rom + ITEM_IMAGES_OFFSET + i * 4);
        item_image_offsets[i][1] = adjust_grom_offset(READ_BE_UINT16(cpu_rom + ITEM_IMAGES_OFFSET + i * 4 + 2));
    }

    /* Extract the title screen text. NUL bytes in the original data
       are converted to newlines for display as a single string. */
    title_screen = MemAlloc(RTPI_TITLE_TEXT_LENGTH);
    memcpy((char *)title_screen, cpu_rom + 0x13db, RTPI_TITLE_TEXT_LENGTH);
    for (int i = 0; i < RTPI_TITLE_TEXT_LENGTH; i++) {
        if (title_screen[i] == 0)
            title_screen[i] = '\n';
    }
    title_screen[RTPI_TITLE_TEXT_LENGTH - 1] = '\0';

    /* Extract the 52-tile font (8 bytes each) used for
       rendering all the graphics. */
    memcpy(ti994atiles, cpu_rom + 0x1cf4, NUMBER_OF_RTPI_TILES * 8);

    free(cpu_rom);

    /* Load the GROM data. Try the single g.bin file first. */
    size_t grom_length;
    uint8_t *grom_data = extract_file_from_zip_data(data, datalength, "g.bin", &grom_length);

    /* If g.bin is not present, this may be the MAME zip format which
       splits the GROM across five 0x2000-byte bank files (g3–g7).
       Extract and concatenate them into a single contiguous buffer. */
    if (!grom_data) {
        grom_data = MemCalloc(GROM_SIZE);
        char filename[14];
        size_t bank_length;
        for (int i = 0; i < 5; i++) {
            snprintf(filename, 14, "phm3189g%d.bin", 3 + i);
            uint8_t *bank = extract_file_from_zip_data(data, datalength, filename, &bank_length);
            if (!bank) {
                free(grom_data);
                return UNKNOWN_GAME;
            }
            memcpy(grom_data + 0x2000 * i, bank, bank_length);
            free(bank);
        }
        grom_length = GROM_SIZE;
    }

    /* I have been unable to find a standard Scott Adams header
       in the data. Instead, all the game parameters are hardcoded here
       to match those in the adv14a.dat file created by Paul David Doherty
       (which was likely created by analyzing the same game data that we
       are dealing with here.) */
    /* See https://unbox.ifarchive.org/?url=/if-archive/scott-adams/games/scottfree/AdamsGames.zip */
    int raw_header[25];

    raw_header[0]  = 4;       // Word length: 4
    raw_header[1]  = 104;     // Number of words: 104
    raw_header[2]  = 277;     // Number of actions: 277
    raw_header[3]  = 71;      // Number of items: 71
    raw_header[4]  = 89;      // Number of messages: 89
    raw_header[5]  = 24;      // Number of rooms: 24
    raw_header[6]  = 10;      // Max carried: 10
    raw_header[7]  = 1;       // Starting location: 1
    raw_header[8]  = 13;      // Number of treasures: 13
    raw_header[9]  = 1;       // Light time: 1
    raw_header[10] = 13 << 8; // Treasure room: 13

    int num_items, num_actions, num_words, num_rooms, max_carry,
        player_room, num_treasures, word_length, light_time,
        num_messages, treasure_room;

    ParseHeader(raw_header, US_HEADER, &num_items, &num_actions, &num_words,
                &num_rooms, &max_carry, &player_room, &num_treasures,
                &word_length, &light_time, &num_messages, &treasure_room);

    GameHeader.NumItems = num_items;
    Items = (Item *)MemAlloc(sizeof(Item) * (num_items + 1));
    GameHeader.NumActions = num_actions;
    Actions = (Action *)MemAlloc(sizeof(Action) * (num_actions + 1));
    GameHeader.NumWords = num_words;
    GameHeader.WordLength = word_length;
    Verbs = MemAlloc(sizeof(char *) * (num_words + 2));
    Nouns = MemAlloc(sizeof(char *) * (num_words + 2));
    GameHeader.NumRooms = num_rooms;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (num_rooms + 1));
    GameHeader.MaxCarry = max_carry;
    GameHeader.PlayerRoom = player_room;
    GameHeader.Treasures = num_treasures;
    GameHeader.LightTime = light_time;
    LightRefill = light_time;
    GameHeader.NumMessages = num_messages;
    Messages = MemAlloc(sizeof(char *) * (num_messages + 1));
    GameHeader.TreasureRoom = treasure_room;

    /* GameInfo struct describing the GROM layout. The offsets here are
       raw byte positions within the GROM file (not GROM addresses —
       adjust_grom_offset has already been applied to image offsets, and
       these offsets are used directly by LoadRTPI). Graphics data is
       handled separately via LoadRTPIGraphics, so the image-related
       fields are all zero. */
    GameInfo info = {
        "Return to Pirate's Isle",
        RETURN_TO_PIRATES_ISLE,
        US_VARIANT,                 // type
        ENGLISH,                   // subtype
        FOUR_LETTER_UNCOMPRESSED, // dictionary type

        71,  // Number of items
        277, // Number of actions
        104, // Number of words
        26,  // Number of rooms
        10,  // Max carried items
        4,   // Word length
        89,  // Number of messages

        103, // number_of_verbs
        105, // number_of_nouns

        0,   // no header
        NO_HEADER,  // no header style

        0,   // no room images
        0,   // no item flags
        0,   // no item images

        0x8772, // actions
        UNCOMPRESSED,
        0x6d9c,  // dictionary
        0x71b6,  // start_of_room_descriptions
        0x0106,  // start_of_room_connections
        0x7391,  // start_of_messages
        FOLLOWS, // start_of_item_descriptions
        0x869A,  // start_of_item_locations

        0, // start_of_system_messages
        0, // start of directions

        0, // start_of_characters
        0, // start_of_image_data
        0, // image_address_offset
        0, // number_of_pictures
        0, // palette
        0, // picture_format_version
    };

    PrintHeaderInfo(raw_header, num_items, num_actions, num_words, num_rooms,
                    max_carry, player_room, num_treasures, word_length,
                    light_time, num_messages, treasure_room);

    /* Parse all game data from the GROM, then load graphics and set up
       the TI-99/4A colour table. The original zip data is freed here
       since it's no longer needed once the GROM has been loaded. */
    if (LoadRTPI(grom_data, grom_length, info, 1) == RETURN_TO_PIRATES_ISLE) {
        free(data);
        LoadRTPIGraphics(room_image_offsets, item_image_offsets, grom_data);
        free(grom_data);
        SetupRTPIColors();

        return RETURN_TO_PIRATES_ISLE;
    }

    free(grom_data);

    return UNKNOWN_GAME;
}

/* Override the default Scott Adams system messages with the text used
   by the TI-99/4A version of Return to Pirate's Isle. These differ
   from the standard messages in wording and punctuation. */
void UpdateRTPISystemMessages(void) {
    sys[TOO_DARK_TO_SEE] = "I can't SEE!\n";
    sys[YOU_SEE] = ". Some visible items: ";
    sys[EXITS] = "Obvious exits: ";
    sys[YOU_FELL_AND_BROKE_YOUR_NECK] = "Fell & Thanks a lot; I'm Dead! ";
    sys[DANGEROUS_TO_MOVE_IN_DARK] = "Dangerous move! ";
    sys[WHAT_NOW] = "-> Command me: ";
    sys[YOUVE_SOLVED_IT] = "Congrats! You did it!\n";
}
