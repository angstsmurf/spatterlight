//
//  graphics.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Palette code based on code by David Lodge
//  with help from Paul David Doherty (including the colour code)
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//  See also http://aimemorial.if-legends.org/gfxbdp.html
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "palette.h"
#include "taylor.h"
#include "taylordraw.h"
#include "utility.h"
#include "irmak.h"

#include "graphics.h"

glui32 pixel_size;
glui32 x_offset;

int last_image_index;

static uint8_t *EndOfGraphicsData;

#pragma mark Some common functions

void PutPixel(glsi32 x, glsi32 y, int32_t color)
{
    int y_offset = 0;

    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
        y * pixel_size + y_offset, pixel_size, pixel_size);
}

void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color)
{
    if (!Graphics)
        return;
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
        y * pixel_size, width * pixel_size, height * pixel_size);
}

/* There is an unused image (46) of the cannon
 on the road (room 34) but it has the wrong background
 colour. We fix that here.
 */
void PatchAndDrawQP3Cannon(void)
{
    DrawPictureNumber(46, 1);
    imagebuffer[IRMAK_IMGWIDTH * 8 + 25][8] &= 191;
    imagebuffer[IRMAK_IMGWIDTH * 9 + 25][8] &= 191;
    imagebuffer[IRMAK_IMGWIDTH * 9 + 26][8] &= 191;
    imagebuffer[IRMAK_IMGWIDTH * 9 + 27][8] &= 191;
    DrawIrmakPictureFromBuffer();
}


#pragma mark Patching

struct image_patch
{
    GameIDType id;
    int picture_number;
    int offset;
    int number_of_bytes;
    const char *patch;
};

static const struct image_patch image_patches[] = {
    { UNKNOWN_GAME, 0, 0, 0, "" },
    { QUESTPROBE3, 55, 604, 3, "\xff\xff\x82" },
    { QUESTPROBE3, 56, 357, 46, "\x79\x81\x78\x79\x7b\x83\x47\x79\x82\x78\x79\x7b\x83\x47\x79\x83"
        "\x78\x79\x7b\x81\x79\x47\x79\x84\x7b\x83\x47\x79\x84\x58\x83\x47"
        "\x7a\x84\x5f\x18\x81\x5f\x47\x50\x84\x5f\x18\x81\x5f\x47" },
    { NUMGAMES, 0, 0, 0, "" },
};

static int FindImagePatch(GameIDType game, int image_number, int start)
{
    for (int i = start + 1; image_patches[i].id != NUMGAMES; i++) {
        if (image_patches[i].id == game && image_patches[i].picture_number == image_number) {
            return i;
        }
    }
    return 0;
}

static void Patch(uint8_t *offset, int patch_number)
{
    const struct image_patch *patch = &image_patches[patch_number];
    for (int i = 0; i < patch->number_of_bytes; i++) {
        const char newval = patch->patch[i];
        offset[i + patch->offset] = (uint8_t)newval;
    }
}

#pragma mark Extract images from file

static void ExtractSingleQ3Image(Image *img, int picture_number, size_t base, size_t offsets, size_t imgdata)
{
    img->imagedata = NULL;

    uint16_t offset_addr = (FileImage[base + picture_number] & 0x7f) * 2 + offsets;
    if (offsets >= FileImageLen)
        return;
    uint16_t image_addr = imgdata + FileImage[offset_addr] + FileImage[offset_addr + 1] * 256;
    if (image_addr >= FileImageLen - 4)
        return;

    uint8_t *pos = &FileImage[image_addr];
    img->width = *pos++;
    img->height = *pos++;
    img->xoff = *pos++;
    img->yoff = *pos++;
    img->imagedata = pos;
    img->datasize = EndOfGraphicsData - pos;
    
    /* In some Spectrum versions, the "wind tunnel" image
       is damaged. As the game contains several (wasteful)
       copies of this image and different ones are broken in
       different ways, we just make use the data for number 17
       for all of them (after patching it.)*/
    if (picture_number == 17) {
        img->imagedata = MemAlloc(608);
        img->datasize = 608;
        memcpy(img->imagedata, pos, MIN(EndOfGraphicsData - pos, 608));
        int patch = FindImagePatch(QUESTPROBE3, 55, 0);
        Patch(img->imagedata, patch);
    } else if (picture_number == 55 || (picture_number >= 18 && picture_number <= 20)) {
        img->imagedata = images[17].imagedata;
        img->datasize = images[17].datasize;
    } else if (picture_number == 56) {
        /* The last image, 56, the egg and the bio gem,
           is broken in many versions, especially the colours.
           We fix it here. */
        img->imagedata = MemAlloc(403);
        img->datasize = 403;
        memcpy(img->imagedata, pos, MIN(EndOfGraphicsData - pos, 403));
        int patch = FindImagePatch(QUESTPROBE3, 56, 0);
        Patch(img->imagedata, patch);
    }
}

static void ExtractQ3Images(Image *img)
{
    size_t base = FindCode("\x00\x01\x01\x02\x03\x04\x05\x06\x02\x02", 0, 10);
    size_t offsets = FindCode("\x00\x00\xa7\x02\xa7\x03\xb9\x08\xd7\x0b", 0, 10);
    size_t imgdata = FindCode("\x20\x0c\x00\x00\x8a\x01\x44\xa0\x17\x8a", offsets, 10);

    for (int picture_number = 0; picture_number < Game->number_of_pictures; picture_number++) {
        ExtractSingleQ3Image(img, picture_number, base, offsets, imgdata);
        img++;
    }
}

static void RepeatOpcode(uint8_t *instructions, uint8_t repeatcount, int *index)
{
    int i = *index - 1;
    instructions[i++] = 0x82;
    instructions[i++] = repeatcount;
    instructions[i++] = 0;
    *index = i;
}

static size_t FindTilesStart(void)
{
    /* Look for the tile data */
    size_t pos = FindCode("\x00\x00\x00\x00\x00\x00\x00\x00\x80\x80\x80\x80\x80\x80\x80\x80\x40\x40\x40\x40\x40\x40\x40\x40", 0, 24);
    if (pos == -1) {
        fprintf(stderr, "Cannot find tile data.\n");
        return 0;
    }
    debug_print("Found tiles at pos %zx\n", pos);
    return pos;
}

/* The image format stores width and height in a single byte, so max width is 16. */
/* Blizzard Pass cheats and adds 16 to the width of a hardcoded list of images. */
static void AdjustBlizzardPassImageWidth(Image *img, int picture_number)
{
    switch (picture_number) {
        case 13:
        case 15:
        case 17:
        case 34:
        case 85:
        case 111:
            img->width += 16;
            break;
        default:
            break;
    }
}

#define MAX_INSTRUCTIONS 2048

/* A number of repeating patterns in the image drawing code (may) have been
   replaced with single tokens. Here we replace any tokens with their
   corresponding patterns. */
static int DecompressHemanType(uint8_t *instructions, uint8_t **outpos)
{
    int index = 0;
    uint8_t *pos = *outpos;
    size_t patterns_lookup = Game->image_patterns_lookup + FileBaselineOffset;
    do {
        if (index >= MAX_INSTRUCTIONS) {
            index = MAX_INSTRUCTIONS - 1;
            break;
        }
        instructions[index++] = *pos;
        for (int i = 0; i < Game->number_of_patterns; i++) {
            /* Look for a pattern marker at pos, and if found,
               insert the corresponding pattern into the instructions array. */
            if (patterns_lookup + i < FileImageLen &&
                *pos == FileImage[patterns_lookup + i]) {
                index--;
                size_t baseoffset = patterns_lookup + Game->number_of_patterns + i * 2;
                if (baseoffset >= FileImageLen - 1)
                    break;
                size_t newoffset = FileImage[baseoffset] + FileImage[baseoffset + 1] * 256 - 0x4000 + FileBaselineOffset;
                while (newoffset < FileImageLen && FileImage[newoffset] != Game->pattern_end_marker) {
                    instructions[index++] = FileImage[newoffset++];
                }
                break;
            }
        }
        pos++;
    } while (*pos != 0xfe);

    *outpos = pos;
    return index;
}


/* A number of common "repeat next byte n times" instruction patterns
   beginning with 0x82 have been replace with single tokens. Token 0xfa
   means "copy the next n bytes m times, which is complicated by the fact
   that those n bytes may themselves contain repeat tokens, so we call
   this function again recursively with the bytes to copy.

   See http://aimemorial.if-legends.org/gfxbdp.html and also
   PerformTileTranformations() in irmak.c for more information
   about how repetitions in the tile-based graphics are encoded.
 */

static int DecompressEarlierType(uint8_t *instructions, uint8_t **outpos, int recursion_guard)
{
    int index = 0;
    uint8_t *pos = *outpos;
    do {
        if (index >= MAX_INSTRUCTIONS) {
            index = MAX_INSTRUCTIONS - 1;
            break;
        }
        instructions[index++] = *pos;
        switch (*pos) {
            case 0xef:
                RepeatOpcode(instructions, 1, &index);
                break;
            case 0xee:
                RepeatOpcode(instructions, 2, &index);
                break;
            case 0xeb:
                RepeatOpcode(instructions, 3, &index);
                break;
            case 0xf3:
                pos++;
                RepeatOpcode(instructions, *pos, &index);
                break;
            case 0xfa:
                if (recursion_guard)
                    break;
                index--;
                pos++;
                int numbytes = *pos - 1;
                pos++;
                int copies = *pos;
                uint8_t *copied_bytes = MemAlloc(numbytes + 1);
                memcpy(copied_bytes, pos + 1, numbytes);
                uint8_t *stored_pointer = copied_bytes;
                copied_bytes[numbytes] = 0xfe;
                for (int i = 0; i < copies; i++) {
                    index += DecompressEarlierType(&instructions[index], &copied_bytes, 1);
                    if (index >= MAX_INSTRUCTIONS) {
                        break;
                    }
                    copied_bytes = stored_pointer;
                }
                free(copied_bytes);
                break;
            default:
                break;
        }
        pos++;
    } while (*pos != 0xfe);

    *outpos = pos;
    return index;
}

static uint8_t *ExtractImage(Image *img, uint8_t *pos) {
    uint8_t instructions[MAX_INSTRUCTIONS];
    int instructions_length;
    if (Version == HEMAN_TYPE) {
        instructions_length = DecompressHemanType(instructions, &pos);
    } else {
        instructions_length = DecompressEarlierType(instructions, &pos, 0);
    }
    instructions[instructions_length++] = 0xfe;
    
    img->imagedata = MemAlloc(instructions_length);
    img->datasize = instructions_length;
    memcpy(img->imagedata, instructions, instructions_length);
    return pos + 1;
}

void InitGraphics(void)
{
    if (images != NULL)
        return;

    if (Game->number_of_pictures == 0) {
        NoGraphics = 1;
        return;
    }

    EndOfGraphicsData = FileImage + FileImageLen;

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        fprintf(stderr, "InitGraphics: invalid palette. Entering text-only mode.\n");
        Game->number_of_pictures = 0;
        NoGraphics = 1;
        return;
    }

    DefinePalette();

    size_t tiles_start = FindTilesStart();
    if (!tiles_start)
        tiles_start = Game->start_of_tiles + FileBaselineOffset;
#ifdef DRAWDEBUG
    debug_print(stderr, "tiles_start: %zx (%zu)\n", Game->start_of_tiles + FileBaselineOffset, Game->start_of_tiles + FileBaselineOffset);
#endif
    uint8_t *pos;
    int numgraphics = Game->number_of_pictures;
    pos = SeekToPos(FileImage, tiles_start);

#ifdef DRAWDEBUG
    debug_print("Grabbing tile details\n");
    debug_print("Tile Offset: %04x\n", tiles_start - file_baseline_offset);
#endif
    for (int i = 0; i < 246; i++) {
        for (int y = 0; y < 8 && pos < EndOfGraphicsData; y++) {
            tiles[i][y] = *pos++;
        }
    }

    /* Now we have hopefully read the tile data */
    /* Time for the image offsets */

    images = (Image *)MemAlloc(sizeof(Image) * numgraphics);
    Image *img = images;
    size_t image_blocks_start_address = Game->start_of_image_blocks + FileBaselineOffset;

    pos = SeekToPos(FileImage, image_blocks_start_address);


    if (Version == QUESTPROBE3_TYPE) {
        ExtractQ3Images(images);
    } else {
        for (int picture_number = 0; picture_number < numgraphics; picture_number++) {
            uint8_t widthheight = *pos++;
            img->width = ((widthheight & 0xf0) >> 4) + 1;
            img->height = (widthheight & 0x0f) + 1;
            if (CurrentGame == BLIZZARD_PASS) {
                /* The image format stores width and height in a single byte,
                   so max width is 16. Blizzard Pass cheats and adds 16 to
                   the width of a hardcoded list of images. */
                AdjustBlizzardPassImageWidth(img, picture_number);
            }
            pos = ExtractImage(img, pos);
            img++;
        }
        /* Questprobe 3 has no Taylor image data,
           (it uses the older type of graphics)
           so we only set this for the other games. */
        InitTaylor(&FileImage[Game->start_of_image_instructions + FileBaselineOffset],
                       EndOfGraphicsData - 5, ObjectLoc, Version != HEMAN_TYPE, BaseGame == REBEL_PLANET, NULL);
    }

    InitIrmak(Game->number_of_pictures, 4);
}
