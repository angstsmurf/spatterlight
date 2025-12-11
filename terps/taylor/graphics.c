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
#include "taylor.h"
#include "taylordraw.h"
#include "utility.h"
#include "irmak.h"

#include "graphics.h"

static Image *images = NULL;

glui32 pixel_size;
glui32 x_offset;

static uint8_t *EndOfGraphicsData;

/* palette handler stuff starts here */

typedef uint8_t RGB[3];
typedef RGB PALETTE[16];
PALETTE pal;

palette_type palchosen = NO_PALETTE;

#define INVALIDCOLOR 16

//#define DRAWDEBUG

static void SetColor(int32_t index, RGB *colour)
{
    pal[index][0] = (*colour)[0];
    pal[index][1] = (*colour)[1];
    pal[index][2] = (*colour)[2];
}

void DefinePalette(void)
{
    /* set up the palette */
    if (palchosen == VGA) {
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 255 };
        RGB red = { 255, 0, 0 };
        RGB magenta = { 255, 0, 255 };
        RGB green = { 0, 255, 0 };
        RGB cyan = { 0, 255, 255 };
        RGB yellow = { 255, 255, 0 };
        RGB white = { 255, 255, 255 };
        RGB brblack = { 0, 0, 0 };
        RGB brblue = { 0, 0, 255 };
        RGB brred = { 255, 0, 0 };
        RGB brmagenta = { 255, 0, 255 };
        RGB brgreen = { 0, 255, 0 };
        RGB brcyan = { 0, 255, 255 };
        RGB bryellow = { 255, 255, 0 };
        RGB brwhite = { 255, 255, 255 };

        SetColor(0, &black);
        SetColor(1, &blue);
        SetColor(2, &red);
        SetColor(3, &magenta);
        SetColor(4, &green);
        SetColor(5, &cyan);
        SetColor(6, &yellow);
        SetColor(7, &white);
        SetColor(8, &brblack);
        SetColor(9, &brblue);
        SetColor(10, &brred);
        SetColor(11, &brmagenta);
        SetColor(12, &brgreen);
        SetColor(13, &brcyan);
        SetColor(14, &bryellow);
        SetColor(15, &brwhite);
    } else if (palchosen == ZX) {
        /* corrected Sinclair ZX palette (pretty dull though) */
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 154 };
        RGB red = { 154, 0, 0 };
        RGB magenta = { 154, 0, 154 };
        RGB green = { 0, 154, 0 };
        RGB cyan = { 0, 154, 154 };
        RGB yellow = { 154, 154, 0 };
        RGB white = { 154, 154, 154 };
        RGB brblack = { 0, 0, 0 };
        RGB brblue = { 0, 0, 170 };
        RGB brred = { 186, 0, 0 };
        RGB brmagenta = { 206, 0, 206 };
        RGB brgreen = { 0, 206, 0 };
        RGB brcyan = { 0, 223, 223 };
        RGB bryellow = { 239, 239, 0 };
        RGB brwhite = { 255, 255, 255 };

        SetColor(0, &black);
        SetColor(1, &blue);
        SetColor(2, &red);
        SetColor(3, &magenta);
        SetColor(4, &green);
        SetColor(5, &cyan);
        SetColor(6, &yellow);
        SetColor(7, &white);
        SetColor(8, &brblack);
        SetColor(9, &brblue);
        SetColor(10, &brred);
        SetColor(11, &brmagenta);
        SetColor(12, &brgreen);
        SetColor(13, &brcyan);
        SetColor(14, &bryellow);
        SetColor(15, &brwhite);
    } else if (palchosen == ZXOPT) {
        /* optimized but not realistic Sinclair ZX palette (SPIN emu) */
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 202 };
        RGB red = {
            202,
            0,
            0,
        };
        RGB magenta = { 202, 0, 202 };
        RGB green = { 0, 202, 0 };
        RGB cyan = { 0, 202, 202 };
        RGB yellow = { 202, 202, 0 };
        RGB white = { 202, 202, 202 };
        /*
         old David Lodge palette:

         RGB black = { 0, 0, 0 };
         RGB blue = { 0, 0, 214 };
         RGB red = { 214, 0, 0 };
         RGB magenta = { 214, 0, 214 };
         RGB green = { 0, 214, 0 };
         RGB cyan = { 0, 214, 214 };
         RGB yellow = { 214, 214, 0 };
         RGB white = { 214, 214, 214 };
         */
        RGB brblack = { 0, 0, 0 };
        RGB brblue = { 0, 0, 255 };
        RGB brred = { 255, 0, 20 };
        RGB brmagenta = { 255, 0, 255 };
        RGB brgreen = { 0, 255, 0 };
        RGB brcyan = { 0, 255, 255 };
        RGB bryellow = { 255, 255, 0 };
        RGB brwhite = { 255, 255, 255 };

        SetColor(0, &black);
        SetColor(1, &blue);
        SetColor(2, &red);
        SetColor(3, &magenta);
        SetColor(4, &green);
        SetColor(5, &cyan);
        SetColor(6, &yellow);
        SetColor(7, &white);
        SetColor(8, &brblack);
        SetColor(9, &brblue);
        SetColor(10, &brred);
        SetColor(11, &brmagenta);
        SetColor(12, &brgreen);
        SetColor(13, &brcyan);
        SetColor(14, &bryellow);
        SetColor(15, &brwhite);
    } else if ((palchosen == C64A) || (palchosen == C64B)) {
        /* and now: C64 palette (pepto/VICE) */
        RGB black = { 0, 0, 0 };
        RGB white = { 255, 255, 255 };
        RGB red = { 191, 97, 72 };
        RGB cyan = { 153, 230, 249 };
        RGB purple = { 177, 89, 185 };
        RGB green = { 121, 213, 112 };
        RGB blue = { 95, 72, 233 };
        RGB yellow = { 247, 255, 108 };
        RGB orange = { 186, 134, 32 };
        RGB brown = { 116, 105, 0 };
        RGB lred = { 231, 154, 132 };
        RGB dgrey = { 69, 69, 69 };
        RGB grey = { 167, 167, 167 };
        RGB lgreen = { 192, 255, 185 };
        RGB lblue = { 162, 143, 255 };
        RGB lgrey = { 200, 200, 200 };

        SetColor(0, &black);
        SetColor(1, &white);
        SetColor(2, &red);
        SetColor(3, &cyan);
        SetColor(4, &purple);
        SetColor(5, &green);
        SetColor(6, &blue);
        SetColor(7, &yellow);
        SetColor(8, &orange);
        SetColor(9, &brown);
        SetColor(10, &lred);
        SetColor(11, &dgrey);
        SetColor(12, &grey);
        SetColor(13, &lgreen);
        SetColor(14, &lblue);
        SetColor(15, &lgrey);
    }
}

const char *colortext(int32_t col)
{
    const char *zxcolorname[] = {
        "black",
        "blue",
        "red",
        "magenta",
        "green",
        "cyan",
        "yellow",
        "white",
        "bright black",
        "bright blue",
        "bright red",
        "bright magenta",
        "bright green",
        "bright cyan",
        "bright yellow",
        "bright white",
        "INVALID",
    };

    const char *c64colorname[] = {
        "black",
        "white",
        "red",
        "cyan",
        "purple",
        "green",
        "blue",
        "yellow",
        "orange",
        "brown",
        "light red",
        "dark grey",
        "grey",
        "light green",
        "light blue",
        "light grey",
        "INVALID",
    };

    if (palchosen >= C64A)
        return (c64colorname[col]);
    else
        return (zxcolorname[col]);
}

int32_t Remap(int32_t color)
{
    int32_t mapcol;

    if ((palchosen == ZX) || (palchosen == ZXOPT)) {
        /* nothing to remap here; shows that the gfx were created on a ZX */
        mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);
    } else if (palchosen == C64A) {
        /* remap A determined from Golden Baton, applies to S1/S3/S13 too (8col) */
        int32_t c64remap[] = {
            0, // black
            6, // blue
            2, // red
            4, // magenta
            5, // green
            3, // cyan
            7, // yellow
            1, // white
            0, // bright black
            6, // bright blue
            2, // bright red
            4, // bright magenta
            5, // bright green
            3, // bright cyan
            7, // bright yellow
            1, // bright white
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else if (palchosen == C64B) {
        /* remap B determined from Spiderman (16col) */
        int32_t c64remap[] = {
            0,
            6,
            9,
            4,
            5,
            12,
            8,
            15,
            0,
            14,
            2,
            10,
            13,
            3,
            7,
            1,
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else
        mapcol = (((color >= 0) && (color <= 15)) ? color : INVALIDCOLOR);

    return (mapcol);
}

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
    glui32 glk_color = ((pal[color][0] << 16)) | ((pal[color][1] << 8)) | (pal[color][2]);

    glk_window_fill_rect(Graphics, glk_color, x * pixel_size + x_offset,
        y * pixel_size, width * pixel_size, height * pixel_size);
}

void ClearGraphMem(void)
{
    memset(imagebuffer, 0, IRMAK_IMGSIZE * 9);
}

void PatchAndDrawQP3Cannon(void)
{
    DrawPictureNumber(46);
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
                                "\x78\x79\x7b\x81\x79\x47\x79\x84\x7b\x83\x47\x79\x84\x58\x83\x47\x7a\x84\x5f\x18\x81\x5f"
                                "\x47\x50\x84\x5f\x18\x81\x5f\x47" },
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

    /* Fixes to broken original image data */
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
            tiles[i][y] = *(pos++);
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
        InitTaylorData(&FileImage[Game->start_of_image_instructions + FileBaselineOffset],
                       EndOfGraphicsData - 5);
    }
}

#pragma mark Irmak picture accessor functions

void DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
                             int xoff, int yoff, size_t datasize, int draw_to_buffer) {
    IrmakImgContext ctx;
    ctx.dataptr = dataptr;
    ctx.xsize = xsize;
    ctx.ysize = ysize;
    ctx.xoff = xoff;
    ctx.yoff = yoff;
    ctx.version = 4;
    ctx.datasize = datasize;
    ctx.draw_to_buffer = draw_to_buffer;

    DrawIrmakPictureFromContext(ctx);
}

void DrawPictureNumber(int picture_number)
{
    DrawPictureAtPos(picture_number, -1, -1, 1);
}

void DrawPictureAtPos(int picture_number, int x, int y, int draw_to_buffer)
{
    if (NoGraphics || Game->number_of_pictures == 0)
        return;
    if (picture_number >= Game->number_of_pictures) {
        debug_print("Invalid image number %d! Last image:%d\n", picture_number,
                    Game->number_of_pictures - 1);
        return;
    }

    Image img = images[picture_number];
    if (img.imagedata == NULL)
        return;

    if (x < 0 || x > IRMAK_IMGWIDTH) {
        x = img.xoff;
    }
    if (y < 0 || y > IRMAK_IMGHEIGHT) {
        y = img.yoff;
    }

    DrawSagaPictureFromData(img.imagedata, img.width, img.height, x, y, img.datasize, draw_to_buffer);
}

#pragma mark Debug

void PrintImageContents(int index, uint8_t *data, size_t size)
{
    debug_print("/* image %d ", index);
    debug_print(
                "width: %d height: %d xoff: %d yoff: %d size: %zu bytes*/\n{ ",
                images[index].width, images[index].height, images[index].xoff,
                images[index].yoff, size);
    for (int i = 0; i < size; i++) {
        debug_print("0x%02x, ", data[i]);
        if (i % 8 == 7)
            debug_print("\n  ");
    }

    debug_print(" },\n");
    return;
}
