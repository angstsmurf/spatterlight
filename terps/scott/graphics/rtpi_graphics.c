/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  rtpi_graphics.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2026-02-12.
 *
 *  Graphics decoder for "Return to Pirate's Isle" (RTPI), a Scott Adams
 *  adventure originally released for the TI-99/4A. The loader is in
 *  rtpi_loader.c.
 *
 *  Graphics use the TI-99/4A's VDP (Video Display Processor) tile
 *  system: 256 screen positions arranged as 32 columns × 12 rows of
 *  8×8-pixel tiles. Image data is stored in GROM as a bytecode stream
 *  of 8 opcodes (0–7) that manipulate a 52-tile font with rotations
 *  and bitwise compositing (OR/AND/XOR), followed by RLE-compressed
 *  colour data. Colours use the TMS9918A's attribute format: one byte
 *  per pixel row, upper nibble = foreground, lower nibble = background.
 */

#include <string.h>

#include <stdlib.h>
#include "scott.h"
#include "scott_display.h"
#include "irmak.h"

#include "rtpi_graphics.h"

/* VDP screen buffer: 32 columns × 12 rows × 8 bytes per tile = 0xC00 bytes.
   vdp_pixels holds 1-bpp tile pattern data; vdp_colors holds per-row
   colour attributes (upper nibble = fg, lower nibble = bg). */
#define RTPI_IMAGE_SIZE (32 * 12 * 8)

static uint8_t vdp_pixels[RTPI_IMAGE_SIZE];
static uint8_t vdp_colors[RTPI_IMAGE_SIZE];

/* The 52-entry tile font, loaded from the CPU ROM (c.bin). */
uint8_t rtpi_tile_font[NUM_RTPI_TILES][8];

/* Debug names for the 8 draw opcodes. */
static const char *draw_opcode_names[8] = {
    "op0_copy_tiles",
    "op1_repeat_tile",
    "op2_move_cursor",
    "op3_copy_one_tile",
    "op4_decode_and_blit_tiles",
    "op5_copy_tile_run",
    "op6_sub_opcodes",
    "op7_combine_tile_pairs"
};

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
static int DecodeRTPIColors(uint8_t *ptr, USImage *image) {
    debug_print("DecodeRTPIColors\n");
    uint8_t *origptr = image->imagedata;
    uint8_t *origcolptr = ptr;
    (void)origcolptr; /* read by debug_print only */
    uint8_t *end = origptr + image->datasize;

    uint16_t write_offset = 0;

    while (ptr < end && write_offset < RTPI_IMAGE_SIZE) {
        uint8_t draw_op = *ptr++;

        uint8_t top    = draw_op & 0xf0;
        uint8_t bottom = draw_op & 0x0f;

        if (top == 0x70) {
            if (ptr >= end)
                break;
            uint8_t newaddr = *ptr++;
            write_offset = ((bottom << 8) | newaddr) * 8;
            debug_print("DecodeRTPIColors: move opcode (0x70). New write offset: 0x%04x\n", write_offset);
            if (write_offset >= RTPI_IMAGE_SIZE) {
                debug_print("DecodeRTPIColors: write offset 0x%04x is offscreen. Max is 0x%04x. (read offset: 0x%lx) Bailing\n", write_offset, RTPI_IMAGE_SIZE, ptr - origcolptr);
                return 0;
            }
            continue;
        }
        if (top != 0 && draw_op != 0) {
            debug_print("DecodeRTPIColors: Drew 0x%02x to 0x%04x.\n", draw_op, write_offset);
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
        debug_print("DecodeRTPIColors: Writing color 0x%02x %d times.\n", color, repeats);
        for (int i = 0; i < repeats; i++) {
            if (write_offset >= RTPI_IMAGE_SIZE) {
                debug_print("DecodeRTPIColors: write offset is offscreen: 0x%04x, max 0x%04x.\n", write_offset, RTPI_IMAGE_SIZE);
                return 0;
            }
            vdp_colors[write_offset++] = color;
        }
    }

    size_t used = ptr - origptr;
    if (used < image->datasize) {
        image->imagedata = MemRealloc(image->imagedata, used);
        image->datasize = used;
        LoadTrace("DecodeRTPIColors: Pruned size of %s %d to 0x%zx\n", image->usage == IMG_ROOM ? "IMG_ROOM" : "IMG_ROOM_OBJ", image->index, used);
    }
    return 1;
}

/* Copy a tile from the font and apply rotation. Special case: tile 0
   with rotation 1 produces a solid black (all-bits-set) tile rather
   than a rotated blank. */
static int get_tile_and_rotate(uint8_t *tile, int tile_index, int rotation) {
    if (tile_index >= NUM_RTPI_TILES) {
        debug_print("get_tile_and_rotate: illegal tile %d\n", tile_index);
        return 0;
    }
    memcpy(tile, rtpi_tile_font[tile_index], 8);
    /* Index 0 with rotation 1 means a black tile */
    if (tile_index == 0 && rotation == 1) {
        memset(tile, 0xff, 8);
    } else {
        RotateTile(tile, rotation);
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
   signals the transition to the colour attribute section.

   Each opcode has a handler below, working on the decoder state. */
typedef struct {
    uint8_t *gptr; /* read position in the bytecode */
    uint8_t *gend;
    uint8_t *dst; /* write position in vdp_pixels */
    uint16_t tile_index;
    uint16_t screen_index;
    int rotation;
    int repeats;
    uint8_t tile[8];
    uint8_t worktile[8];
} RTPIDecoder;

/* Opcode 0. Copy tiles: read from VDP, compose, blit, repeat.
 * For each repeat, reads a tile from VDP into the buffer,
 * blits the buffer to VDP at R4, clears the buffer, and reads
 * the next tile index from GROM for the next iteration.
 */
static void RTPICopyTiles(RTPIDecoder *d) {
    for (int i = 0; i < d->repeats; i++) {
        if (!copy_tile_from_vdp(d->tile, d->screen_index))
            break;
        d->dst = blit_tile(d->dst, d->tile);
        if (i < d->repeats - 1) {
            if (d->gptr >= d->gend)
                break;
            d->screen_index = *d->gptr++;
        }
    }
}

/* Opcode 1. Repeat tile: draw tile with rotation, blit repeatedly.
 * Processes a single tile with full rotation/composition,
 * then blits the result to VDP (repeats) times. Each blit writes the
 * same composed tile to consecutive VDP positions.
 */
static void RTPIRepeatTile(RTPIDecoder *d) {
    if (!get_tile_and_rotate(d->tile, d->tile_index, d->rotation))
        return;

    for (int i = 0; i < d->repeats; i++) {
        d->dst = blit_tile(d->dst, d->tile);
    }
    memset(d->tile, 0, 8);
}

/* Opcode 3. Copy one tile: read tile from VDP and blit with repeats.
 * Reads one tile from VDP into the buffer, then blits
 * it to VDP (repeat) times.
 */
static void RTPICopyOneTile(RTPIDecoder *d) {
    if (!copy_tile_from_vdp(d->tile, d->screen_index))
        return;
    for (int i = 0; i < d->repeats; i++) {
        d->dst = blit_tile(d->dst, d->tile);
    }
    memset(d->tile, 0, 8);
}

/* Opcode 4. Process tile with rotation, blit, clear, repeat.
 * Decode and blit tiles. For each iteration, reads a tile
 * index from GROM, processes it with rotation, blits to VDP,
 * clears the buffer, and repeats for (repeats) iterations.
 */
static void RTPIDecodeTiles(RTPIDecoder *d) {
    for (int i = 0; i < d->repeats; i++) {
        if (!get_tile_and_rotate(d->tile, d->tile_index, d->rotation))
            continue;
        d->dst = blit_tile(d->dst, d->tile);
        if (i < d->repeats - 1) {
            if (!get_next_byte(&d->gptr, d->gend, &d->tile_index, &d->rotation))
                break;
        }
    }
    memset(d->tile, 0, 8);
}

/* Opcode 5. Copy tile run from VDP.
 * Reads a tile from VDP, blits it, increments the tile
 * index, clears the buffer, and repeats. This draws a
 * sequential run of tiles (tile N, N+1, N+2, ...).
 */
static void RTPICopyTileRun(RTPIDecoder *d) {
    for (int i = 0; i < d->repeats; i++) {
        if (!copy_tile_from_vdp(d->tile, d->screen_index))
            break;
        d->dst = blit_tile(d->dst, d->tile);
        d->screen_index++;
    }
    memset(d->tile, 0, 8);
}

/* Opcode 6. Sub-opcodes.
 * One of 16 sub-opcodes depending on
 * the value of `repeats`. Only the ones actually
 * used by the game are implemented here.
 */
static void RTPISubOpcode(RTPIDecoder *d) {
    if (d->repeats > 0 && d->repeats < 13) {
        if (!get_tile_and_rotate(d->worktile, d->tile_index, d->rotation))
            return;
    }

    switch(d->repeats) {
        case 0: /* 0. Write 8 raw bytes to screen memory directly from GROM */
            if (d->gptr + 8 > d->gend || d->dst + 8 > vdp_pixels + RTPI_IMAGE_SIZE)
                break;
            for (int i = 0; i < 8; i++) {
                *d->dst++ = *d->gptr++;
            }
            break;
        case 1: /* 1-4. OR tile_index with next (repeats) tiles, then blit and clear */
        case 2:
        case 3:
        case 4:
            for (int i = 0; i < d->repeats; i++) {
                for (int j = 0; j < 8; j++)
                    d->tile[j] |= d->worktile[j];
                if (i < d->repeats - 1) {
                    if (!get_next_byte(&d->gptr, d->gend, &d->tile_index, &d->rotation))
                        break;
                    if (!get_tile_and_rotate(d->worktile, d->tile_index, d->rotation))
                        break;
                }
            }
            d->dst = blit_tile(d->dst, d->tile);
            memset(d->tile, 0, 8);
            break;
        case 6: /* 6. Clear, AND tile_index with next tile, blit and clear */
            if (!get_next_byte(&d->gptr, d->gend, &d->tile_index, &d->rotation))
                break;
            if (!get_tile_and_rotate(d->tile, d->tile_index, d->rotation))
                break;
            for (int i = 0; i < 8; i++)
                d->tile[i] &= d->worktile[i];
            d->dst = blit_tile(d->dst, d->tile);
            memset(d->tile, 0, 8);
            break;
        case 9: /* 9. AND tile with tile_index */
            for (int i = 0; i < 8; i++) {
                d->tile[i] &= d->worktile[i];
            }
            break;
        case 10: /* 10. OR tile with tile_index */
            for (int i = 0; i < 8; i++) {
                d->tile[i] |= d->worktile[i];
            }
            break;
        case 11: /* 11. XOR tile with tile_index */
            for (int i = 0; i < 8; i++) {
                d->tile[i] ^= d->worktile[i];
            }
            break;
        case 13: /* 13. Copy screen tile at screen_index to tile */
            copy_tile_from_vdp(d->tile, d->screen_index);
            break;
        case 15: /* 15. Blit tile and copy tile_index to tile */
            d->dst = blit_tile(d->dst, d->tile);
            if (!get_tile_and_rotate(d->tile, d->tile_index, 0))
                memset(d->tile, 0, 8);
            break;
        default:
            fprintf(stderr, "DrawRTPIImageWithOptionalFuzz: Unimplemented sub-opcode (%d)\n", d->repeats);
            break;
    }
}

/* Opcode 7. Combine tile pairs.
 * OR two tiles and blit them.
 * Repeat (repeats / 2) times.
 */
static void RTPICombineTilePairs(RTPIDecoder *d) {
    for (int i = 0; i < d->repeats; i++) {
        if (!get_tile_and_rotate(d->worktile, d->tile_index, d->rotation))
            break;
        for (int j = 0; j < 8; j++)
            d->tile[j] |= d->worktile[j];
        if (i % 2 == 1) {
            d->dst = blit_tile(d->dst, d->tile);
            memset(d->tile, 0, 8);
        }
        if (i < d->repeats - 1) {
            if (!get_next_byte(&d->gptr, d->gend, &d->tile_index, &d->rotation))
                break;
        }
    }
}

static int DrawRTPIImageWithOptionalFuzz(USImage *image, int fuzzy) {

    if (image == NULL || image->imagedata == NULL || image->datasize < 2) {
        return 0;
    }

    debug_print("DrawRTPIImageWithOptionalFuzz: drawing image %d\n", image->index);

    /* Clear colour memory for full room images to avoid residual
       colours from the previous image. Don't clear for overlay
       images (room objects) drawn on top of a base image. */
    if (image->usage == IMG_ROOM && image->index <= GameHeader.NumRooms) {
        ClearVDP();
    }

    RTPIDecoder decoder = { 0 };
    RTPIDecoder *d = &decoder;
    d->dst = vdp_pixels;

    /* Fuzzy mode: skip 2 bytes to misalign the bytecode stream,
       producing a garbled image (simulates myopia). */
    d->gptr = image->imagedata + (fuzzy ? 2 : 0);
    d->gend = image->imagedata + image->datasize;

    while (d->gptr + 2 <= d->gend) {
        /*   - Bits 0-5: tile index. There are only 52 tile patterns.
         *   - Bits 6-7: rotation mode (0=none, 1=rot-90, 2=rot-180, 3=rot-270)
         *   - Some opcodes (such as 0) instead use bits 0-8 as the index to an
         *     already drawn tile in video memory (screen_index).
         *   - Bits 9-12: repeat count / subop index
         *   - Bits 13-15: opcode
         */
        uint16_t draw_ops_word = READ_BE_UINT16(d->gptr);
        d->gptr += 2;
        d->tile_index = draw_ops_word & 0x3f;
        d->screen_index = draw_ops_word & 0x1ff;
        d->rotation = (draw_ops_word & 0xc0) >> 6;
        d->repeats = (draw_ops_word & 0x1e00) >> 9;
        int op = (draw_ops_word & 0xe000) >> 13;

        if (d->repeats == 0) {
            if (op == 7) {
                debug_print("DrawRTPIImageWithOptionalFuzz: repeats is 0 and opcode is 7: Jump to DecodeRTPIColors.\n");
                return DecodeRTPIColors(d->gptr - 1, image);
            }

            if (op != 2) {
                d->gptr--;
                memset(d->tile, 0, 8);
                if (op != 6)
                    continue;
            }
        }

        debug_print("DrawRTPIImage: tile index: %d rot:%d rep:%d op:%d\n", d->tile_index, d->rotation, d->repeats, op);

        (void)draw_opcode_names; /* read by debug_print only */
        debug_print("DrawRTPIImage: draw op: %s\n", draw_opcode_names[op]);

        switch (op) {
            case 0:
                RTPICopyTiles(d);
                break;
            case 1:
                RTPIRepeatTile(d);
                break;
            case 2:
                /* Move the write address to a tile position, without
                 * writing any tile data.
                 */
                if (d->screen_index * 8 < RTPI_IMAGE_SIZE)
                    d->dst = vdp_pixels + d->screen_index * 8;
                break;
            case 3:
                RTPICopyOneTile(d);
                break;
            case 4:
                RTPIDecodeTiles(d);
                break;
            case 5:
                RTPICopyTileRun(d);
                break;
            case 6:
                RTPISubOpcode(d);
                break;
            case 7:
                RTPICombineTilePairs(d);
                break;
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
void SetupRTPIColors(void) {

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

/* Make any image black by clearing the vdp color data. */
void ClearVDP(void) {
    memset(vdp_colors, 0, RTPI_IMAGE_SIZE);
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
                if (PixelSet(byte, p)) {
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
