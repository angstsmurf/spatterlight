//
//  c64_small_graphics.c
//  scott
//
//  Created by Administrator on 2026-01-23.
//

// Handles the compact C64 graphics format used by early Scott Adams
// SAGA adventures (Pirate Adventure, Voodoo Castle) where game database
// and image data are bundled in a single file. Room backgrounds use
// RLE-compressed multicolor bitmaps; small item images are stored
// as C64 hardware sprites (24x21 monochrome).

#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "saga.h"
#include "memory_allocation.h"
#include "sagagraphics.h"
#include "c64a8draw.h"

#include "c64_small.h"

/* Screen position for drawing a C64 sprite overlay */
typedef struct {
    int index; // object index this sprite represents
    int x;     // pixel x position on screen
    int y;     // pixel y position on screen
} spritelist;

/* Pirate Adventure sprite positions: maps object index to screen coordinates.
   Items sharing coordinates are drawn at the same spot (e.g. all bottles). */
static const spritelist sprites[] = {
    {25, 104,  62}, // Bottle of rum
    {7,  104,  62}, // Empty bottle
    {42, 104,  62}, // Bottle of salt water
    {44, 86,   62}, // sneakers
    {46, 152,  46}, // shovel
    {60, 184,  62}, // sack of crackers
    {8,  152,  46}, // torch
    {63, 152,  46}, // burnt-out torch
    {9,  152,  46}, // lit torch
    {21, 152,  46}, // fish
    {31, 167,  34}, // hammer
    {15, 152,  46}, // anchor
    {3,  105,  46}, // book
    {10, 104,  30}, // matches
    {5,  208,  46}, // duffel bag
    {14, 152,  49}, // mongoose
    {24, 171,  55}, // parrot
    {33, 152,  56}, // precut lumber
    {20, 189,  46}, // pile of sails
    {0,    0,   0}  // sentinel
};

/* Image registry for Pirate Adventure C64: maps object/room indices to
   byte offsets within the all-in-one file. Object images come first
   (both RLE bitmaps and raw sprite data), then room backgrounds. */
static const imglist listPirate[] = {
    /* RLE bitmap object overlays */
    { IMG_ROOM_OBJ, 17, 0x0302 }, // (17) (Mean and hungry looking crocodiles
    { IMG_ROOM_OBJ, 30, 0x0447 }, // (30) Rug
    { IMG_ROOM_OBJ, 41, 0x0491 }, // (41) Sleeping pirate
    { IMG_ROOM_OBJ, 12, 0x0519 }, // (12) Wicked looking pirate
    { IMG_ROOM_OBJ, 37, 0x05e7 }, // (37) Pirate ship
    { IMG_ROOM_OBJ, 19, 0x073c }, // (19) Open door with hall beyond
    { IMG_ROOM_OBJ,  4, 0x0788 }, // (4) Bookcase with secret passage beyond
    { IMG_ROOM_OBJ, 28, 0x08a3 }, // (28) Open treasure chest
    { IMG_ROOM_OBJ, 22, 0x09a6 }, // (22) DOUBLOONS
    { IMG_ROOM_OBJ, 23, 0x09fa }, // (23) Deadly mamba snakes

    /* Raw 63-byte C64 sprite data (24x21 monochrome, 3 bytes per row) */
    { IMG_ROOM_OBJ, 25, 0x1d02 }, // (23) Bottle of rum
    { IMG_ROOM_OBJ, 44, 0x1d42 }, // (23) Safety sneakers
    { IMG_ROOM_OBJ, 46, 0x1d82 }, // (45) Shovel
    { IMG_ROOM_OBJ, 60, 0x1dc2 }, // (23) Sack of crackers
    { IMG_ROOM_OBJ,  8, 0x1e02 }, // (23) unlit torch
    { IMG_ROOM_OBJ, 21, 0x1e42 }, // (21) fish
    { IMG_ROOM_OBJ, 31, 0x1e82 }, // (31) claw hammer
    { IMG_ROOM_OBJ, 15, 0x1ec2 }, // (15) anchor
    { IMG_ROOM_OBJ,  3, 0x1f02 }, // (3) blood-soaked book
    { IMG_ROOM_OBJ, 10, 0x1f42 }, // (10) Matches
    { IMG_ROOM_OBJ,  5, 0x1f82 }, // (5) Pirate's duffel bag
    { IMG_ROOM_OBJ, 14, 0x1fc2 }, // (14) Mongoose
    { IMG_ROOM_OBJ, 24, 0x2002 }, // (9) Parrot
    { IMG_ROOM_OBJ, 33, 0x2042 }, // (33) Pile of precut lumber
    { IMG_ROOM_OBJ, 20, 0x2082 }, // (20) Pile of sails

    /* Room background images (RLE-compressed multicolor bitmaps) */
    { IMG_ROOM, 1, 0x2102 }, //  (1) Room in
    { IMG_ROOM, 2, 0x22c4 }, //  (2) alcove
    { IMG_ROOM, 3, 0x2517 }, //  (3) Secret passageway
    { IMG_ROOM, 4, 0x2730 }, //  (4) musty attic
    { IMG_ROOM, 5, 0x288b }, //  (5) *I'M outside an open window
    { IMG_ROOM, 6, 0x2bc5 }, //  (6) sandy beach on a tropical isle"
    { IMG_ROOM, 7, 0x2e17 }, //  (7) maze of caves"
    { IMG_ROOM, 8, 0x313c }, //  (8) meadow"
    { IMG_ROOM, 9, 0x3369 }, //  (9) grass shack"
    { IMG_ROOM, 10, 0x365a }, //  (10) *I'm in the ocean"
    { IMG_ROOM, 11, 0x37ed }, //  (11) pit"
    { IMG_ROOM, 12, 0x3a31 }, //  (12) maze of caves"
    { IMG_ROOM, 13, 0x3ce1 }, //  (13) maze of caves"
    { IMG_ROOM, 14, 0x3fb8 }, //  (14) *I'm at the foot of a cave ridden hill
    { IMG_ROOM, 15, 0x4289 }, //  (15) tool shed"
    { IMG_ROOM, 16, 0x43f7 }, //  (16) long hallway"
    { IMG_ROOM, 17, 0x463b }, //  (17) large cavern"
    { IMG_ROOM, 18, 0x490c }, //  (18) *I'm on top of a hill.
    { IMG_ROOM, 19, 0x4c28 }, //  (19) maze of caves"
    { IMG_ROOM, 20, 0x4f06 }, //  (20) *I'm aboard Pirate ship anchored off shore"
    { IMG_ROOM, 21, 0x52ad }, //  (21) *I'm on the beach at *Treasure* Island"
    { IMG_ROOM, 22, 0x5555 }, //  (22) spooky old graveyard filled with piles
    { IMG_ROOM, 23, 0x587b }, //  (23) large barren field"
    { IMG_ROOM, 24, 0x5aed }, //  (24) shallow lagoon.
    { IMG_ROOM, 25, 0x5c93 }, //  (25) sacked and deserted monastary"
    { IMG_ROOM, 26, 0x5eee }, //  (26) *Welcome to Never Never Land"
    { 0, 0, 0, }
};

/* Image registry for Voodoo Castle C64. Same format as Pirate Adventure. */
static const imglist listVoodoo[] = {
    { IMG_ROOM_OBJ, 37, 0x02 }, // (73) Closed Safe
    { IMG_ROOM_OBJ, 34, 0x65 }, // (72) Open SAfe
    { IMG_ROOM_OBJ, 17, 0x41b }, // (31) Closed Coffin
    { IMG_ROOM_OBJ, 31, 0x0125 }, // (71) Wide crack in the wall
    { IMG_ROOM_OBJ, 41, 0x0191 }, // (68) Wide open door
    { IMG_ROOM_OBJ, 6, 0x02aa }, // (74) Dark hole
    { IMG_ROOM_OBJ, 23, 0x0360 }, // (64) Spirit Medium
    { IMG_ROOM_OBJ, 28, 0x048f }, // (65) Slick chute leading downward
    { IMG_ROOM, 1, 0x1c02 }, // (0) chapel
    { IMG_ROOM, 2, 0x1f2e }, // (2) Dingy Looking Stairwell
    { IMG_ROOM, 3, 0x20a3 }, // (3) room in the castle
    { IMG_ROOM, 4, 0x244a }, // (4) Tunnel
    { IMG_ROOM, 5, 0x2828 }, // (5) room in the castle
    { IMG_ROOM, 6, 0x2abe }, // (6) *I'm in Medium Maegen's Mad Room
    { IMG_ROOM, 7, 0x2d29 }, // (7) room in the castle
    { IMG_ROOM, 8, 0x2ff5 }, // (8) room in the castle
    { IMG_ROOM, 9, 0x3174 }, // (9) room in the castle
    { IMG_ROOM, 10, 0x346c }, // (10) Ballroom
    { IMG_ROOM, 11, 0x3682 }, // (11) dungeon
    { IMG_ROOM, 12, 0x38ef }, // (12) *I'm in the Armory
    { IMG_ROOM, 13, 0x3d21 }, // (13) torture chamber
    { IMG_ROOM, 14, 0x3f73 }, // (14) Chimney
    { IMG_ROOM, 15, 0x43e5 }, // (15) Large Fireplace
    { IMG_ROOM, 16, 0x47d0 }, // (16) room in the castle
    { IMG_ROOM, 17, 0x4b74 }, // (17) Lab
    { IMG_ROOM, 18, 0x4e6a }, // (18) narrow part of the chimney
    { IMG_ROOM, 19, 0x520f }, // (19) Graveyard
    { IMG_ROOM, 20, 0x5553 }, // (20) parlor
    { IMG_ROOM, 21, 0x5726 }, // (21) Jail Cell
    { IMG_ROOM, 22, 0x5a56 }, // (22) *I'm on a ledge
    { IMG_ROOM, 23, 0x5cca }, // (23) hidden VOODOO room
    { IMG_ROOM, 24, 0x5fff }, // (24) room in the castle
    { IMG_ROOM, 25, 0x62fc }, // (25) lot of TROUBLE!
    { 0, 0, 0, }
};

/* Clone an existing image entry under a different object index.
   Used when multiple objects share the same sprite data (e.g. all
   three bottle variants reuse the rum bottle sprite). */
static USImage *copyToImage(USImage *copy, USImage *original, int index) {
    USImage *previous = copy->previous;
    memcpy(copy, original, sizeof(USImage));
    copy->index = index;
    copy->previous = previous;
    copy->next = NewImage();
    copy->next->previous = copy;
    return copy->next;
}

/* Pirate Adventure reuses some sprites for multiple objects:
   - Bottle of rum (#25) -> empty bottle (#7), salt water bottle (#42)
   - Unlit torch (#8) -> burnt-out torch (#63), lit torch (#9)
   - Rug (#30) -> map/plans (#26)
   This creates the duplicate image entries so each object has its own
   entry in the USImages list, pointing at the shared sprite data. */
static USImage *add_dupliceate_image_entries(USImage *image) {
    USImage *image2 = USImages;
    USImage *bottle = NULL;
    USImage *torch = NULL;
    USImage *rug = NULL;

    while (image2 != image) {
        if (image2->usage == IMG_ROOM_OBJ) {
            if (image2->index == 25)  {
                bottle = image2;
            } else if (image2->index == 8) {
                torch = image2;
            } else if (image2->index == 30) {
                rug = image2;
            }
        }
        image2 = image2->next;
    }
    if (bottle != NULL) {
        image = copyToImage(image, bottle, 7);  // empty bottle
        image = copyToImage(image, bottle, 42); // salt water bottle
    }
    if (torch != NULL) {
        image = copyToImage(image, torch, 63); // burnt-out torch
        image = copyToImage(image, torch, 9);  // lit torch
    }
    if (rug != NULL) {
        image = copyToImage(image, rug, 26); // map/plans
    }
    return image;
}

/* Load an all-in-one C64 file that bundles the game database and image
   data together. Splits the file at imgoffset: the portion after the
   cutoff becomes the game database (returned via *sf / *extent), while
   the image data before and after the cutoff is extracted into USImages
   using the game-specific registry table. */
GameIDType handle_all_in_one(uint8_t **sf, size_t *extent, c64rec c64_registry)
{
    int cutoff = c64_registry.imgoffset;
    if (cutoff == 0 || cutoff >= *extent) {
        return UNKNOWN_GAME;
    }

    uint8_t *data = *sf;

    /* Extract the game database portion (everything after the image data) */
    size_t smallsize = *extent - cutoff;
    uint8_t *shorter = MemAlloc(smallsize);
    memcpy(shorter, data + cutoff, smallsize);

    GameIDType result = LoadBinaryDatabase(shorter, smallsize, *Game, 0);
    if (result == UNKNOWN_GAME) {
        free(shorter);
        return UNKNOWN_GAME;
    }

    int outpic;

    const imglist *list = NULL;

    switch (CurrentGame) {
        case PIRATE_US:
            list = listPirate;
            break;
        case VOODOO_CASTLE_US:
            /* listVoodoo is calibrated for the canonical crack's memory layout
               (object/room images in 0..0x6c30, database at imgoffset 0x6c30).
               The c64.com release (imgoffset 0x3800, database at $4000) is a
               stripped crack with no image data anywhere in its depacked RAM,
               so gate the table to the layout it matches and load the rest
               text-only. */
            if (cutoff == 0x6c30)
                list = listVoodoo;
            break;
        default:
            /* COUNT_US and any other all-in-one game without an image table:
               the database parsed fine above, so ship it playable as text.
               (The c64.com Count crack contains no image data at all.) */
            break;
    }

    if (list == NULL) {
        /* Text-only: hand back just the database portion. */
        free(*sf);
        *sf = shorter;
        *extent = smallsize;
        CurrentSys = SYS_C64;
        return CurrentGame;
    }

    USImages = NewImage();
    USImage *image = USImages;

    /* Color 0 is always black */
    SetColor(0, 0);

    /* Walk the registry and extract each image into the USImages linked list.
       Image size is determined by the gap between consecutive offsets. */
    for (outpic = 0; list[outpic].offset != 0; outpic++) {
        size_t offset = list[outpic].offset;
        size_t nextoffset = list[outpic + 1].offset;

        size_t size;
        if (nextoffset < offset) {
            /* Last entry or wrap-around: image extends to end of file */
            size = *extent - offset;
        } else {
            size = nextoffset - offset;
        }

        if (size > *extent) {
            break;
        }
        image->systype = SYS_C64_TINY;
        image->usage = list[outpic].usage;
        image->index = list[outpic].index;
        image->imagedata = MemAlloc(size);
        image->datasize = size;

        memcpy(image->imagedata, data + offset, size);

        image->next = NewImage();
        image->next->previous = image;
        image = image->next;
    }
    if (image->imagedata == NULL) {
        /* Trim the trailing empty node from the linked list */
        if (image != USImages) {
            if (CurrentGame == PIRATE_US) {
                /* Add entries for objects that share sprites with other objects */
                image = add_dupliceate_image_entries(image);
            }
            image->previous->next = NULL;
            free(image);
            image = NULL;
        } else {
            free(USImages);
            USImages = NULL;
        }
    }

    /* Safety check: discard the list if it ended up empty */
    if (USImages != NULL && USImages->next == NULL && USImages->imagedata == NULL) {
        free(USImages);
        USImages = NULL;
    }

    ImageWidth = 320;
    if (CurrentGame == VOODOO_CASTLE_US)
        ImageHeight = 80;
    else
        ImageHeight = 78;

    /* Prepend a hard-coded all-black "darkness" ROOM image at index 0.
       The RLE body fills the rect with palette-index-0 pixels
       (which is black after the initial SetColor(0, 0) above). */
    USImage *background = NewImage();
    background->systype = SYS_C64_TINY;
    background->usage = IMG_ROOM;
    background->index = 0;
    background->datasize = 8 + 7 * 3;
    background->imagedata = MemCalloc(background->datasize);
    background->imagedata[0] = 11;                    /* left  = 88  / 8 */
    background->imagedata[1] = 0;                     /* top */
    background->imagedata[2] = 29;                    /* right = 232 / 8 */
    background->imagedata[3] = (uint8_t)ImageHeight;  /* bottom */

    /* 7 repeat-128 commands, each emitting 128 pairs of color-0 pixels.
       Comfortably covers 19 columns x up to 41 pairs = 779 pairs. */
    for (int i = 0; i < 7; i++) {
        // This leaves two zero bytes after every 0xff.
        background->imagedata[8 + i * 3] = 0xff;
    }
    background->next = USImages;
    if (USImages != NULL)
        USImages->previous = background;
    USImages = background;

    /* Replace the original all-in-one buffer with just the database portion */
    free(*sf);
    *sf = shorter;
    *extent = smallsize;

    return CurrentGame;
}

/* Standard Commodore 64 color palette (16 colors, RGB) */
glui32 colorC64[16] = {
    0x000000, // black
    0xffffff, // white
    0xbf6148, // red
    0x99e6f9, // cyan
    0xb159b9, // purple
    0x62D375, // green
    0x5f48e9, // blue
    0xf7ff6c, // yellow
    0xba8620, // orange
    0x877219, // brown
    0xe79a84, // light red
    0x7B7B7B, // light grey
    0xa7a7a7, // grey
    0xc0ffb9, // light green
    0xa28fff, // light blue
    0x454545, // dark grey
};

/* Decode and draw an RLE-compressed multicolor bitmap.
   Returns a pointer past the consumed data, or NULL on error.

   Data format:
     Byte 0:    left column (in character cells, 8px each)
     Byte 1:    top scanline
     Byte 2:    right column
     Byte 3:    bottom scanline
     Bytes 4-7: four palette entries (C64 color indices)
     Remaining: RLE-compressed pixel data (pairs of bytes)

   RLE encoding: a signed count byte followed by pixel data.
     count >= 0: (count+1) literal pairs follow (2 bytes each)
     count <  0: one pair follows, repeated (count & 0x7f)+1 times

   Pixels are drawn in 8-pixel-wide columns, top to bottom. When the
   bottom edge is reached, drawing advances to the next column. */
static uint8_t *DrawMiniC64ImageFromData(uint8_t *ptr, size_t datasize)
{
    if (datasize < 8)
        return NULL;

    uint8_t *endptr = ptr + datasize - 1;

    /* Parse the bounding rectangle (in character-cell coordinates).
       Images with left < 10 are shifted right so all images align to
       column 11, with the right edge extended to match. */
    int shifted = 0;
    int left = *ptr++;
    if (left < 10) {
        shifted = 11 - left;
        left = 11;
    }
    left *= 8;
    int xpos = left;

    int top = *ptr++;
    int ypos = top;

    int right = (*ptr++ + shifted) * 8;

    int bottom = *ptr++;

    /* Read and apply the 4-entry palette (multicolor mode: 2 bits per pixel.) */
    /* Overlay images have all colors set to 0. For these, don't apply the palette –
       these use the palette of the background image */
    if (ptr + 4 > endptr)
        return NULL;
    for (int i = 0; i < 4; i++) {
        int color = *ptr++;
        if (color > 15) {
            fprintf(stderr, "Error Illegal color!\n");
            return NULL;
        } else if (color != 0) {
            SetColor(i, colorC64[color]);
        }
    }

    /* Decode RLE-compressed pixel data */
    uint8_t work1 = 0, work2 = 0;

    while (xpos <= right && ptr < endptr) {
        int8_t count = *ptr++;
        int repeat_pattern = (count < 0);
        if (repeat_pattern) {
            count &= 0x7f;
            work1 = *ptr++;
            work2 = *ptr++;
        }
        for (int i = 0; i <= count; i++) {
            if (!repeat_pattern) {
                if (ptr + 1 > endptr)
                    return ptr;
                work1 = *ptr++;
                work2 = *ptr++;
            }
            /* Draw two bytes of pixel data and advance the cursor */
            xpos = DrawPatternAndAdvancePos(xpos, &ypos, work1);
            xpos = DrawPatternAndAdvancePos(xpos, &ypos, work2);

            if (ypos > bottom) {
                if (xpos == right)
                    return ptr;
                xpos += 8;
                ypos = top;
            }
        }
    }
    return ptr;
}

/* Draw a C64 hardware sprite: 24x21 pixels, monochrome (white).
   Sprite data is 63 bytes (3 bytes per row x 21 rows). Each row is
   a 24-bit bitmask where set bits are drawn as white pixels. */
static int DrawSprite(USImage *img, int index) {
    spritelist sprite = sprites[index];
    SetColor(5, 0xffffff);
    for (int i = 0; i < 63; i += 3) {
        glsi32 y = sprite.y + i / 3;

        uint32_t line = img->imagedata[i] << 16 | img->imagedata[i + 1] << 8 | img->imagedata[i + 2];
        for (int j = 23; j >= 0; j--) {
            if ((line >> j) & 1) {
                PutPixel(sprite.x + (23 - j), y, 5);
            }
        }
    }
    return 1;
}

/* Main entry point for drawing a C64 "tiny" image.
   For Pirate Adventure objects: some bitmaps are location-specific overlays
   that only make sense in one room, so skip them elsewhere. Objects that
   are C64 sprites are dispatched to DrawSprite; everything else goes
   through the RLE bitmap decoder. After decoding, the image buffer is
   shrunk to its actual consumed size. */
int DrawMiniC64(USImage *img) {
    if (CurrentGame == PIRATE_US && img->usage == IMG_ROOM_OBJ) {
        /* Location-specific bitmap overlays: only draw in the correct room */
        switch (img->index) {
            case 26: // rug — only in starting room
                if (MyLoc != 1)
                    return 0;
                break;
            case 12: // wicked pirate — only in shack
                if (MyLoc != 9)
                    return 0;
                break;
            case 28: // Open treasure chest — only in shack
                if (MyLoc != 9)
                    return 0;
                break;
            case 37: // pirate ship — only on beach
                if (MyLoc != 6)
                    return 0;
                break;
            case 41: // sleeping pirate — only in attic
                if (MyLoc != 4)
                    return 0;
                break;
            case 22: // doubloons — only in monastery
                if (MyLoc != 25)
                    return 0;
                break;
        }
        /* Check if this object has a sprite; if so, draw it directly */
        for (int i = 0; sprites[i].index != 0; i++) {
            if (img->index == sprites[i].index) {
                return DrawSprite(img, i);
            }
        }
    }
    uint8_t *result  = DrawMiniC64ImageFromData(img->imagedata, img->datasize);
    if (result == NULL)
        return 0;

    /* Shrink the buffer to the actual decoded size, discarding trailing
       padding that was allocated based on the gap to the next image. */
    size_t actual_size = result - img->imagedata;
    if (actual_size < img->datasize) {
        img->imagedata = MemRealloc(img->imagedata, actual_size);;
        img->datasize = actual_size;
    }
   return 1;
}
