//
//  rtpi_graphics.c
//  scott
//
//  Created by Administrator on 2026-02-12.
//

#include <string.h>
#include <stdlib.h>

#include "scott.h"
#include "irmak.h"
#include "detectgame.h"
#include "sagagraphics.h"
#include "unzip_in_mem.h"

#include "rtpi_graphics.h"

#define RTPI_IMAGE_SIZE 32 * 12 * 8 // 12 rows of 32 8x8 tiles: 0xc00 bytes

static uint8_t vdp_pixels[RTPI_IMAGE_SIZE];
static uint8_t vdp_colors[RTPI_IMAGE_SIZE];

#define GROM_SIZE 0xa000

#define NUMBER_OF_RTPI_ROOM_IMAGES 24 + 9 // 24 rooms plus 9 extra images
#define NUMBER_OF_RTPI_ITEM_IMAGES 7
#define NUMBER_OF_RTPI_IMAGES NUMBER_OF_RTPI_ROOM_IMAGES + NUMBER_OF_RTPI_ITEM_IMAGES
#define NUMBER_OF_RTPI_TILES 52
#define RTPI_TITLE_TEXT_LENGTH 0x1e2

static uint8_t ti994atiles[NUMBER_OF_RTPI_TILES][8];

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

static int DecodeRTPIColors(uint8_t *ptr, uint8_t *origptr) {
    uint16_t write_offset = 0;
    int zeroflag = 0;
    uint8_t draw_op = *ptr++;
    do {
        if (!zeroflag && ((draw_op & 0xf0) != 0 || (draw_op & 0x0f) == 0)) {
            if (draw_op != 0) {
                if ((draw_op & 0xf0) == 0x70) {
                    // Special case: If top nibble of draw_op is 7, jump to new write offset
                    uint8_t newaddr = *ptr++;
                    write_offset = ((draw_op & 0xf) << 8 | newaddr) * 8;
                } else {
                    vdp_colors[write_offset++] = draw_op;
                }
            } else {
                // Special case: Draw op 0 means that the entire next byte is the repeat count
                zeroflag = 1;
            }
        } else {
            // If zeroflag is set (i.e. last draw_op was zero),
            // the entire byte is the repeat count.
            // Otherwise, it is just the lower nibble.
            if (!zeroflag) {
                draw_op &= 0x0f;
            }
            int16_t repeats = draw_op * 8;
            zeroflag = 0;
            uint8_t color = *ptr++;
            for (int i = 0; i < repeats; i++) {
                if (write_offset > RTPI_IMAGE_SIZE) {
                    return 0;
                }
                vdp_colors[write_offset++] = color;
            }
        }
        draw_op = *ptr++;
    } while (write_offset < RTPI_IMAGE_SIZE && !(zeroflag && draw_op == 0));
    debug_print("DecodeRTPIColors: Finished at offset 0x%lx\n", ptr - origptr);
    return 1;
}

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

static uint8_t *get_next_byte(uint8_t *ptr, uint16_t *tile_index, int *rotation) {
    uint8_t grom_byte = *ptr++;
    *tile_index = grom_byte & 0x3f;
    *rotation = grom_byte >> 6;
    return ptr;
}

static uint8_t *blit_tile(uint8_t *dst, uint8_t *tile) {
    if (dst + 8 > vdp_pixels + RTPI_IMAGE_SIZE)
        return dst;
    memcpy(dst, tile, 8);
    return dst + 8;
}


static int DrawRTPIImageWithFuzz(USImage *image, int fuzzy) {

    if (image == NULL || image->imagedata == NULL) {
        return 0;
    }

    // At least one image (the crawlway) will leave the color of the previous image
    // in the bottom right corner unless we clear color memory. On the other hand, we must
    // not clear it when we are drawing an image that is meant to be a detail on top of another,
    // larger image (i.e. a room object image.)
    if (image->usage == IMG_ROOM && image->index <= GameHeader.NumRooms) {
        memset(vdp_colors, 0, RTPI_IMAGE_SIZE);
    }

    uint8_t *dst = vdp_pixels;

    // Adding 2 to the starting offset makes the image "fuzzy"
    // i.e. messes it up in an interesting way,
    // representing the game protagonist's myopia.
    uint8_t *gptr = image->imagedata + 2 * fuzzy;

    int rotation = 0; // 0..3
    int repeats = 0; // 0..15
    int op = 0;

    uint16_t draw_ops_word = 0;
    uint16_t tile_index, screen_index;
    uint8_t tile[8] = {0,0,0,0,0,0,0,0};
    uint8_t worktile[8] = {0,0,0,0,0,0,0,0};

    do {
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

        // Special cases:
        if (repeats == 0) {
            // if repeats is 0 and op is 7, it is time to draw colors
            if (op == 7) {
                return DecodeRTPIColors(gptr - 1, image->imagedata);
            }

            // if repeats is 0 and op is not 2, we need to back up a byte
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
                    memcpy(tile, vdp_pixels + screen_index * 8, 8);
                    dst = blit_tile(dst, tile);
                    if (i < repeats - 1) {
                        uint8_t grom_byte = *gptr++;
                        screen_index = grom_byte;
                    }
                }
                break;
            case 1:
                /* --- 1. Repeat tile. Draw tile with rotation, blit repeatedly ---
                 * Op 1: Processes a single tile with full rotation/composition,
                 * then blits the result to VDP R2 times. Each blit writes the
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
                dst = vdp_pixels + screen_index * 8;
                break;
            case 3:
                /* --- 3. Copy one tile. Read tile from VDP and blit with repeats ---
                 * Op 3: Reads one tile from VDP into the buffer, then blits
                 * it to VDP (repeat) times.
                 */
                memcpy(tile, vdp_pixels + screen_index * 8, 8);
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
                        gptr = get_next_byte(gptr, &tile_index, &rotation);
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
                    memcpy(tile, vdp_pixels + screen_index * 8, 8);
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

                // Most of the sub-ops start by getting and rotating worktile
                // (All except 0, 13 and 15)
                if (repeats > 0 && repeats < 13) {
                    if (!get_tile_and_rotate(worktile, tile_index, rotation))
                        break;
                }

                switch(repeats) {
                    case 0: //  0. Writes 8 raw bytes to screen memory directly from GROM
                        for (int i = 0; i < 8; i++) {
                            *dst++ = *gptr++;;
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
                                gptr = get_next_byte(gptr, &tile_index, &rotation);
                                if (!get_tile_and_rotate(worktile, tile_index, rotation))
                                    break;
                            }
                        }
                        dst = blit_tile(dst, tile);
                        memset(tile, 0, 8);
                        break;
                    case 6: //  6. Clear, AND tile_index with next tile, blit and clear
                        gptr = get_next_byte(gptr, &tile_index, &rotation);
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
                    case 13: // 13. Copy screen tile at tile_index to tile
                        memcpy(tile, vdp_pixels + screen_index * 8, 8);
                        break;
                    case 15: // 15. Blit tile and copy tile_index to tile
                        dst = blit_tile(dst, tile);
                        memcpy(tile, ti994atiles[tile_index], 8);
                        break;
                    default:
                        fprintf(stderr, "DrawRTPIImageWithFuzz: Unimplemented sub-opcode (%d)\n", repeats);
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
                        gptr = get_next_byte(gptr, &tile_index, &rotation);
                    }
                }
                break;
            default:
                fprintf(stderr, "DrawRTPIImageWithFuzz: Illegal opcode %d!\n", op);
        }
    } while (gptr - image->imagedata < image->datasize - 2);
    return 0;
}

int DrawRTPIImage(USImage *image) {
    return DrawRTPIImageWithFuzz(image, 0);
}

int DrawFuzzyRTPIImage(USImage *image) {
    return DrawRTPIImageWithFuzz(image, 1);
}

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

    SetColor(0, black);
    SetColor(1, black);
    SetColor(2, medgreen);
    SetColor(3, lghtgreen);
    SetColor(4, blue);
    SetColor(5, brblue);
    SetColor(6, darkred);
    SetColor(7, cyan);
    SetColor(8, mediumred);
    SetColor(9, lightred);
    SetColor(10, yellow);
    SetColor(11, bryellow);
    SetColor(12, drkgreen);
    SetColor(13, magenta);
    SetColor(14, white);
    SetColor(15, brwhite);
}

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

static USImage *finalize_img_make_new(USImage *image, uint16_t offset, uint16_t size, uint8_t *grom) {

    if (offset >= GROM_SIZE)
        Fatal("Bad image data\n");

    image->imagedata = MemAlloc(size);
    memcpy(image->imagedata, grom + offset, size);

    uint16_t maxrange = offset + size;

    // Skip grom header data
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

static int LoadRTPIGraphics(uint16_t *room_image_offsets, uint16_t item_image_offsets[NUMBER_OF_RTPI_ITEM_IMAGES][2], uint8_t *grom) {
    USImages = NewImage();
    USImage *image = USImages;

    // Create a linked list of USImage structs (one for every image)
    for (int i = 0; i < NUMBER_OF_RTPI_ROOM_IMAGES; i++) {

        if (room_image_offsets[i] == 0)
            continue;

        size_t offset = room_image_offsets[i];

        image->systype = SYS_TI994A;
        image->usage = IMG_ROOM;
        image->index = i;
        if (i == 25) {
            // Give image 25 (Texas Instruments logo) the standard title image index (99)
            image->index = 99;
        } else if (i == 27) {
            // Create a room image for item 29: Inside of boat
            image->index = 29;
            image->usage = IMG_ROOM_OBJ;
        } else if (i == 28) {
            // Create a room image for Item 4: Isle off in distance
            // (The deck at night)
            image->index = 4;
            image->usage = IMG_ROOM_OBJ;
        }

        size_t size;

        if (i == 17) {
            size = 0x183;
        } else if (i == 22) {
            size = 0xf7;
        } else if (i == 26) {
            size = 0xdd4;
        } else if (i < NUMBER_OF_RTPI_ROOM_IMAGES - 1) {
            int j = i + 1;
            size_t next;
            do {
                next = room_image_offsets[j++];
            } while (next < offset);
            size = next - offset;
        } else {
            size = item_image_offsets[0][1] - offset;
        }

        image = finalize_img_make_new(image, offset, size, grom);
    }

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

    if (image->imagedata == NULL) {
        /* Free the last image, it is empty */
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

GameIDType FreeGameResources(void);
uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof);
uint8_t *ParseRooms(uint8_t *ptr, uint8_t *endptr, int number_of_rooms);
uint8_t *ParseItems(uint8_t *ptr, uint8_t *endptr, int number_of_items);

static uint8_t *ReadRTPIDictionary(GameInfo info, uint8_t **pointer, int loud) {
    uint8_t *ptr = *pointer;
    if (info.word_length + 2 > 1024)
        Fatal("Bad word length");
    char dictword[1024];
    char c = 0;
    int wordnum = 0;
    int charindex = 0;

    int nw = info.number_of_words;
    int nv = info.number_of_verbs;
    int nn = info.number_of_nouns;

    for (int i = 0; i < nw + 2; i++) {
        Verbs[i] = ".";
        Nouns[i] = ".";
    }

    do {
        for (int i = 0; i < info.word_length; i++) {
            c = *ptr++;

            if (c == 0 && charindex == 0) {
                c = *ptr++;
            }
            if (c != ' ' && charindex > 0 && dictword[charindex - 1] == ' ') {
                i--;
                charindex--;
            }
            if (c == '*') {
                if (charindex != 0)
                    charindex = 0;
                i = -1;
            }
            dictword[charindex++] = c;
        }
        dictword[charindex] = 0;
        if (wordnum < nn) {
            Nouns[wordnum] = MemAlloc(charindex + 1);
            memcpy((char *)Nouns[wordnum], dictword, charindex + 1);
            if (loud)
                debug_print("Noun %d: \"%s\"\n", wordnum, Nouns[wordnum]);
        } else {
            Verbs[wordnum - nn] = MemAlloc(charindex + 1);
            memcpy((char *)Verbs[wordnum - nn], dictword, charindex + 1);
            if (loud)
                debug_print("Verb %d: \"%s\"\n", wordnum - nn, Verbs[wordnum - nn]);
        }
        wordnum++;

        if (c > 127)
            return ptr;

        charindex = 0;
    } while (wordnum <= nv + nn);

    return ptr;
}

static uint8_t *ParseRTPIActions(uint8_t *ptr, uint8_t *data, size_t datalength, int number_of_actions)
{
    int counter = 0;
    Action *ap = Actions;

    size_t offset = ptr - data;
    size_t offset2;

    int verb, noun, value, value2, plus = 0;
    while (counter <= number_of_actions && offset + counter + plus < datalength) {
        plus = number_of_actions + 1;
        verb = data[offset + counter];
        noun = data[offset + counter + plus];

        ap->Vocab = verb * 150 + noun;

        for (int j = 0; j < 2; j++) {
            plus += number_of_actions + 1;
            value = data[offset + counter + plus];
            plus += number_of_actions + 1;
            value2 = data[offset + counter + plus];
            ap->Subcommand[j] = 150 * value + value2;
        }

        offset2 = offset + 6 * (number_of_actions + 1);
        plus = 0;

        for (int j = 0; j < 5 && offset2 + counter * 2 + plus < datalength; j++) {
            uint16_t offset3 = offset2 + counter * 2 + plus;
            if (offset3 >= 0x97fe) {
                offset3 -= 0x97fe;
            }
            ap->Condition[j] = READ_BE_UINT16(data + offset3);
            ptr = data + offset2 + counter * 2 + plus + 2;
            plus += (number_of_actions + 1) * 2;
        }

        debug_print("Action %d Vocab: %d (Verb:%d/NounOrChance:%d)\n", counter, ap->Vocab,
                    ap->Vocab / 150,  ap->Vocab % 150);
        debug_print("Action %d Condition[0]: %d (%d/%d)\n", counter,
                    ap->Condition[0], ap->Condition[0] % 20, ap->Condition[0] / 20);
        debug_print("Action %d Condition[1]: %d (%d/%d)\n", counter,
                    ap->Condition[1], ap->Condition[1] % 20, ap->Condition[1] / 20);
        debug_print("Action %d Condition[2]: %d (%d/%d)\n", counter,
                    ap->Condition[2], ap->Condition[2] % 20, ap->Condition[2] / 20);
        debug_print("Action %d Condition[3]: %d (%d/%d)\n", counter,
                    ap->Condition[3], ap->Condition[3] % 20, ap->Condition[3] / 20);
        debug_print("Action %d Condition[4]: %d (%d/%d)\n", counter,
                    ap->Condition[4], ap->Condition[4] % 20, ap->Condition[4] / 20);
        debug_print("Action %d Subcommand [0]]: %d (%d/%d)\n", counter, ap->Subcommand[0], ap->Subcommand[0] % 150, ap->Subcommand[0] / 150);
        debug_print("Action %d Subcommand [1]]: %d (%d/%d)\n", counter, ap->Subcommand[1], ap->Subcommand[1] % 150, ap->Subcommand[1] / 150);

        ap++;
        counter++;
    }
    return ptr;
}

static uint8_t *ParseRTPIRoomConnections(uint8_t *ptr, uint8_t *endptr, int number_of_rooms) {
    /* The room connections are ordered by direction, not room, so all the North
     * connections for all the rooms come first, then the South connections, and
     * so on. */
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


static GameIDType LoadRTPI(uint8_t *data, size_t length, GameInfo info, int dict_start, int loud)
{
    uint8_t *ptr;
    uint8_t *endptr = data + length;

    int ct;

#pragma mark dictionary

    ptr = data + info.start_of_dictionary;
    if (ptr > endptr)
        return UNKNOWN_GAME;

    ptr = ReadRTPIDictionary(info, &ptr, loud);

#pragma mark rooms

    ptr = data + info.start_of_room_descriptions;
    if (ptr > endptr)
        return UNKNOWN_GAME;

    ptr = ParseRooms(ptr - 1, endptr, GameHeader.NumRooms);

#pragma mark messages

    ptr = data + info.start_of_messages;
    if (ptr > endptr)
        return UNKNOWN_GAME;

    ct = 0;

    uint8_t stringlength = 0;
    while (ct < GameHeader.NumMessages + 1) {
        stringlength = *ptr++;
        if (stringlength != 0) {
            Messages[ct] = MemAlloc(stringlength + 1);
            memcpy(Messages[ct], ptr, stringlength);
            // Skip grom header
            if (ct == 60) {
                uint8_t *partial = (uint8_t *)Messages[ct];
                memcpy(partial + 18, ptr + 0x814, 7);
                memcpy(Messages[ct], partial, stringlength);
                ptr += 0x802;
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

    if (ptr > endptr)
        return UNKNOWN_GAME;

    ptr = ParseItems(ptr, endptr, GameHeader.NumItems);

#pragma mark item locations

    ptr = data + info.start_of_item_locations;
    if (ptr > endptr)
        return UNKNOWN_GAME;

    ct = 0;
    Item *ip = Items;
    while (ct <= GameHeader.NumItems) {
        ip->Location = *ptr++;
        ip->InitialLoc = ip->Location;
        if (Items[ct].Text && ip->Location < GameHeader.NumRooms && Rooms[ip->Location].Text)
            debug_print("Location of item %d, \"%s\":%d\n", ct, Items[ct].Text, ip->Location);
        ip++;
        ct++;
    }

    ptr = data + info.start_of_actions;
    if (ptr > endptr)
        return UNKNOWN_GAME;

    // Parse actions
    ptr = ParseRTPIActions(ptr, data, length, GameHeader.NumActions);
    if (ptr == NULL || ptr >= endptr) {
        return FreeGameResources();
    }

    ptr = data + info.start_of_room_connections;
    if (ptr > endptr)
        return UNKNOWN_GAME;

    ptr = ParseRTPIRoomConnections(ptr, endptr, GameHeader.NumRooms);

    return info.gameID;
}

static uint16_t adjust_grom_offset(uint16_t offset) {
    if ((offset & 0x8000) != 0) {
        offset -= 0x6000;
    }
    return offset;
}

#define ROOM_IMAGES_OFFSET 0x12
#define ITEM_IMAGES_OFFSET 0x54

GameIDType DetectRTPI(uint8_t *data, size_t datalength) {

    size_t length;

    uint8_t *ptr = extract_file_from_zip_data(data, datalength, "c.bin", &length);

    if (!ptr) {
        ptr = extract_file_from_zip_data(data, datalength, "phm3189c.bin", &length);
        if (!ptr) {
            return UNKNOWN_GAME;
        }
    }

    if (length < 0x1e94) {
        free(ptr);
        return UNKNOWN_GAME;
    }

    // We only need the tile data, image offsets and intro text from this file

    uint16_t room_image_offsets[NUMBER_OF_RTPI_ROOM_IMAGES];

    for (int i = 0; i < NUMBER_OF_RTPI_ROOM_IMAGES; i++) {
        room_image_offsets[i] = adjust_grom_offset(READ_BE_UINT16(ptr + ROOM_IMAGES_OFFSET + i * 2));
    }

    uint16_t item_image_offsets[NUMBER_OF_RTPI_ITEM_IMAGES][2];

    for (int i = 0; i < 7; i++) {
        item_image_offsets[i][0] = READ_BE_UINT16(ptr + ITEM_IMAGES_OFFSET + i * 4);
        item_image_offsets[i][1] = adjust_grom_offset(READ_BE_UINT16(ptr + ITEM_IMAGES_OFFSET + i * 4 + 2));
    }

    // Title screen text
    title_screen = MemAlloc(RTPI_TITLE_TEXT_LENGTH);
    memcpy((char *)title_screen, ptr + 0x13db, RTPI_TITLE_TEXT_LENGTH);
    for (int i = 0; i < RTPI_TITLE_TEXT_LENGTH; i++) {
        if (title_screen[i] == 0)
            title_screen[i] = '\n';
    }
    title_screen[RTPI_TITLE_TEXT_LENGTH - 1] = '\0';

    // Tile data
    memcpy(ti994atiles, ptr + 0x1cf4, 51 * 8);

    free(ptr);
    ptr = extract_file_from_zip_data(data, datalength, "g.bin", &length);

    // If there is no g.bin file in the zip archive,
    // look for the five smaller grom files in the MAME zip version
    // and merge them (if found) to recreate the g.bin file.
    if (!ptr) {
        uint8_t *grom = MemCalloc(GROM_SIZE);
        char filename[14];
        for (int i = 0; i < 5; i++) {
            snprintf(filename, 14, "phm3189g%d.bin", 3 + i);
            ptr = extract_file_from_zip_data(data, datalength, filename, &length);
            if (!ptr) {
                free(grom);
                return UNKNOWN_GAME;
            }
            memcpy(grom + 0x2000 * i, ptr, length);
            free(ptr);
        }
        ptr = grom;
        length = GROM_SIZE;
    }

    int header[25];

    // I failed to find anything resembling a header in the data,
    // so we just hardcode the info here.

    header[0]  = 4;       // Word length: 4
    header[1]  = 104;     // Number of words: 104
    header[2]  = 277;     // Number of actions: 277
    header[3]  = 71;      // Number of items: 71
    header[4]  = 89;      // Number of messages: 89
    header[5]  = 24;      // Number of rooms: 24
    header[6]  = 10;      // Max carried: 10
    header[7]  = 1;       // Starting location: 1
    header[8]  = 13;      // Number of treasures: 13
    header[9]  = 1;       // Light time: 1
    header[10] = 13 << 8; // Treasure room: 13

    int ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm;

    ParseHeader(header, US_HEADER, &ni, &na, &nw, &nr, &mc, &pr, &tr,
                &wl, &lt, &mn, &trm);

    GameHeader.NumItems = ni;
    Items = (Item *)MemAlloc(sizeof(Item) * (ni + 1));
    GameHeader.NumActions = na;
    Actions = (Action *)MemAlloc(sizeof(Action) * (na + 1));
    GameHeader.NumWords = nw;
    GameHeader.WordLength = wl;
    Verbs = MemAlloc(sizeof(char *) * (nw + 2));
    Nouns = MemAlloc(sizeof(char *) * (nw + 2));
    GameHeader.NumRooms = nr;
    Rooms = (Room *)MemAlloc(sizeof(Room) * (nr + 1));
    GameHeader.MaxCarry = mc;
    GameHeader.PlayerRoom = pr;
    GameHeader.Treasures = tr;
    GameHeader.LightTime = lt;
    LightRefill = lt;
    GameHeader.NumMessages = mn;
    Messages = MemAlloc(sizeof(char *) * (mn + 1));
    GameHeader.TreasureRoom = trm;

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

    PrintHeaderInfo(header, ni, na, nw, nr, mc, pr, tr, wl, lt, mn, trm);

    if (LoadRTPI(ptr, length, info, 0, 1) == RETURN_TO_PIRATES_ISLE) {
        free(data);
        LoadRTPIGraphics(room_image_offsets, item_image_offsets, ptr);
        free(ptr);
        SetupRTPIColors();

        return RETURN_TO_PIRATES_ISLE;
    }

    free(ptr);

    return UNKNOWN_GAME;
}

void UpdateRTPISystemMessages(void) {
    sys[TOO_DARK_TO_SEE] = "I can't SEE!\n";
    sys[YOU_SEE] = ". Some visible items: ";
    sys[EXITS] = "Obvious exits: ";
    sys[YOU_FELL_AND_BROKE_YOUR_NECK] = "Fell & Thanks a lot; I'm Dead! ";
    sys[DANGEROUS_TO_MOVE_IN_DARK] = "Dangerous move! ";
    sys[WHAT_NOW] = "-> Command me: ";
    sys[YOUVE_SOLVED_IT] = "Congrats! You did it!\n";
}
