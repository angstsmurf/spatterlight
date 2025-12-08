//
//  sagadraw.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Based on code  by David Lodge
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
#include "utility.h"
#include "irmak.h"

#include "sagadraw.h"

static Image *images = NULL;

glui32 pixel_size;
glui32 x_offset;

static uint8_t *taylor_image_data;
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

/* real code starts here */

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
        y * pixel_size, width * pixel_size,
        height * pixel_size);
}

struct image_patch {
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

static void Q3Init(size_t *base, size_t *offsets, size_t *imgdata)
{
    *base = FindCode("\x00\x01\x01\x02\x03\x04\x05\x06\x02\x02", 0, 10);
    *offsets = FindCode("\x00\x00\xa7\x02\xa7\x03\xb9\x08\xd7\x0b", 0, 10);
    *imgdata = FindCode("\x20\x0c\x00\x00\x8a\x01\x44\xa0\x17\x8a", *offsets, 10);
}

static uint8_t *Q3Image(int imgnum, size_t base, size_t offsets, size_t imgdata)
{
    uint16_t offset_addr = (FileImage[base + imgnum] & 0x7f) * 2 + offsets;
    uint16_t image_addr = imgdata + FileImage[offset_addr] + FileImage[offset_addr + 1] * 256;
    return &FileImage[image_addr];
}

static void RepeatOpcode(int *number, uint8_t *instructions, uint8_t repeatcount)
{
    int i = *number - 1;
    instructions[i++] = 0x82;
    instructions[i++] = repeatcount;
    instructions[i++] = 0;
    *number = i;
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

void SagaSetup(void)
{
    if (images != NULL)
        return;

    if (Game->number_of_pictures == 0) {
        NoGraphics = 1;
        return;
    }

    EndOfGraphicsData = FileImage + FileImageLen;

    int32_t i, y;

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        debug_print("unknown palette\n");
        exit(EXIT_FAILURE);
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
    for (i = 0; i < 246; i++) {
        for (y = 0; y < 8 && pos < EndOfGraphicsData; y++) {
            tiles[i][y] = *(pos++);
        }
    }

    /* Now we have hopefully read the tile data */
    /* Time for the image offsets */

    images = (Image *)MemAlloc(sizeof(Image) * numgraphics);
    Image *img = images;
    size_t image_blocks_start_address = Game->start_of_image_blocks + FileBaselineOffset;

    size_t patterns_lookup = Game->image_patterns_lookup + FileBaselineOffset;

    pos = SeekToPos(FileImage, image_blocks_start_address);

    size_t base = 0, offsets = 0, imgdata = 0;

    if (Version == QUESTPROBE3_TYPE)
        Q3Init(&base, &offsets, &imgdata);

    for (int picture_number = 0; picture_number < numgraphics; picture_number++) {
        if (Version == QUESTPROBE3_TYPE) {
            pos = Q3Image(picture_number, base, offsets, imgdata);
            if (pos > EndOfData - 4 || pos < FileImage) {
                fprintf(stderr, "Image %d out of range!\n", picture_number);
                img->imagedata = NULL;
                img++;
                continue;
            }
            img->width = *pos++;
            img->height = *pos++;
            img->xoff = *pos++;
            img->yoff = *pos++;
            img->imagedata = pos;
            img->datasize = EndOfGraphicsData - pos;
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
            img++;
            continue;
        }

        uint8_t widthheight = *pos++;
        img->width = ((widthheight & 0xf0) >> 4) + 1;
        img->height = (widthheight & 0x0f) + 1;
        if (CurrentGame == BLIZZARD_PASS) {
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
        uint8_t instructions[2048];
        int number = 0;
        uint8_t *copied_bytes = NULL;
        uint8_t *stored_pointer = NULL;
        do {
            instructions[number++] = *pos;
            uint8_t opcode = *pos;
            if (Version != HEMAN_TYPE) {
                switch (opcode) {
                case 0xfb:
                    number--;
                    if (!copied_bytes || copied_bytes[0] == 0) {
                        pos = stored_pointer;
                        if (copied_bytes != NULL)
                            free(copied_bytes);
                        copied_bytes = NULL;
                    } else {
                        copied_bytes[0]--;
                        pos = copied_bytes;
                    }
                    break;
                case 0xef:
                    RepeatOpcode(&number, instructions, 1);
                    break;
                case 0xee:
                    RepeatOpcode(&number, instructions, 2);
                    break;
                case 0xeb:
                    RepeatOpcode(&number, instructions, 3);
                    break;
                case 0xf3:
                    pos++;
                    RepeatOpcode(&number, instructions, *pos);
                    break;
                case 0xfa:
                    number--;
                    pos++;
                    int numbytes = *pos++;
                    stored_pointer = pos;
                    if (copied_bytes != NULL)
                        free(copied_bytes);
                    copied_bytes = MemAlloc(numbytes + 1);
                    memcpy(copied_bytes, pos, numbytes);
                    copied_bytes[0]--;
                    copied_bytes[numbytes] = 0xfb;
                    pos = copied_bytes;
                    break;
                }
            } else if (Game->number_of_patterns) {
                for (i = 0; i < Game->number_of_patterns; i++) {
                    if (*pos == FileImage[patterns_lookup + i]) {
                        number--;
                        size_t baseoffset = patterns_lookup + Game->number_of_patterns + i * 2;
                        size_t newoffset = FileImage[baseoffset] + FileImage[baseoffset + 1] * 256 - 0x4000 + FileBaselineOffset;
                        while (FileImage[newoffset] != Game->pattern_end_marker) {
                            instructions[number++] = FileImage[newoffset++];
                        }
                        break;
                    }
                }
            }
            pos++;
        } while (*pos != 0xfe);

        instructions[number++] = 0xfe;

        img->imagedata = MemAlloc(number);
        img->datasize = number;
        memcpy(img->imagedata, instructions, number);

        pos++;
        img++;
    }

    taylor_image_data = &FileImage[Game->start_of_image_instructions + FileBaselineOffset];
}

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

#pragma mark Taylorimage

static void mirror_area(int x1, int y1, int width, int y2)
{
    for (int line = y1; line < y2; line++) {
        int source = line * IRMAK_IMGWIDTH + x1;
        int target = source + width - 1;
        for (int col = 0; col < width / 2; col++) {
            imagebuffer[target][8] = imagebuffer[source][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[target][pixrow] = imagebuffer[source][pixrow];
            Flip(imagebuffer[target]);
            source++;
            target--;
        }
    }
}

static void mirror_top_half(void)
{
    for (int line = 0; line < IRMAK_IMGHEIGHT / 2; line++) {
        for (int col = 0; col < IRMAK_IMGWIDTH; col++) {
            imagebuffer[(11 - line) * IRMAK_IMGWIDTH + col][8] =
            imagebuffer[line * IRMAK_IMGWIDTH + col][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[(11 - line) * IRMAK_IMGWIDTH + col][7 - pixrow] =
                imagebuffer[line * IRMAK_IMGWIDTH + col][pixrow];
        }
    }
}

static void flip_image_horizontally(void)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line < IRMAK_IMGHEIGHT; line++) {
        for (int col = 32; col > 0; col--) {
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[line * IRMAK_IMGWIDTH + col - 1][pixrow] = imagebuffer[line * IRMAK_IMGWIDTH + (32 - col)][pixrow];
            Flip(mirror[line * IRMAK_IMGWIDTH + col - 1]);
        }
    }

    memcpy(imagebuffer, mirror, IRMAK_IMGSIZE * 9);
}

static void flip_image_vertically(void)
{
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line < IRMAK_IMGHEIGHT; line++) {
        for (int col = 0; col < IRMAK_IMGWIDTH; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[(11 - line) * IRMAK_IMGWIDTH + col][7 - pixrow] =
                imagebuffer[line * IRMAK_IMGWIDTH + col][pixrow];
            mirror[(11 - line) * IRMAK_IMGWIDTH + col][8] =
            imagebuffer[line * IRMAK_IMGWIDTH + col][8];
        }
    }
    memcpy(imagebuffer, mirror, IRMAK_IMGSIZE * 9);
}

static void flip_area_vertically(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    //    fprintf(stderr, "flip_area_vertically x1: %d: y1: %d width: %d y2 %d\n", x1, y1, width, y2);
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = 0; line <= y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                mirror[(y2 - line) * IRMAK_IMGWIDTH + col][7 - pixrow] = imagebuffer[(y1 + line) * IRMAK_IMGWIDTH + col][pixrow];
            mirror[(y2 - line) *IRMAK_IMGWIDTH + col][8] = imagebuffer[(y1 + line) * IRMAK_IMGWIDTH + col][8];
        }
    }
    for (int line = y1; line <= y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[line * IRMAK_IMGWIDTH + col][pixrow] =
                mirror[line * IRMAK_IMGWIDTH + col][pixrow];
            imagebuffer[line * IRMAK_IMGWIDTH + col][8] =
            mirror[line * IRMAK_IMGWIDTH + col][8];
        }
    }
}

static void mirror_area_vertically(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    for (int line = 0; line <= y2 / 2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            imagebuffer[(y2 - line) * IRMAK_IMGWIDTH + col][8] =
            imagebuffer[(y1 + line) * IRMAK_IMGWIDTH + col][8];
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[(y2 - line) * IRMAK_IMGWIDTH + col][7 - pixrow] =
                imagebuffer[(y1 + line) * IRMAK_IMGWIDTH + col][pixrow];
        }
    }
}

static void flip_area_horizontally(uint8_t x1, uint8_t y1, uint8_t width, uint8_t y2)
{
    //    fprintf(stderr, "flip_area_horizontally x1: %d: y1: %d width: %d y2 %d\n", x1, y1, width, y2);
    uint8_t mirror[IRMAK_IMGSIZE][9];

    for (int line = y1; line < y2; line++) {
        for (int col = 0; col < width; col++) {
            for (int pixrow = 0; pixrow < 9; pixrow++)
                mirror[line * IRMAK_IMGWIDTH + x1 + col][pixrow] =
                imagebuffer[line * IRMAK_IMGWIDTH + (x1 + width - 1) - col][pixrow];
            Flip(mirror[line * IRMAK_IMGWIDTH + x1 + col]);
        }
    }

    for (int line = y1; line < y2; line++) {
        for (int col = x1; col < x1 + width; col++) {
            for (int pixrow = 0; pixrow < 8; pixrow++)
                imagebuffer[line * IRMAK_IMGWIDTH + col][pixrow] = mirror[line * IRMAK_IMGWIDTH + col][pixrow];
            imagebuffer[line * IRMAK_IMGWIDTH + col][8] = mirror[line * IRMAK_IMGWIDTH + col][8];
        }
    }
}

static void draw_colour_old(uint8_t x, uint8_t y, uint8_t colour, uint8_t length)
{
    for (int i = 0; i < length; i++) {
        imagebuffer[y * IRMAK_IMGWIDTH + x + i][8] = colour;
    }
}

static void draw_colour(uint8_t colour, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            imagebuffer[(y + h) * IRMAK_IMGWIDTH + x + w][8] = colour;
        }
    }
}

static void make_light(void)
{
    for (int i = 0; i < IRMAK_IMGSIZE; i++) {
        imagebuffer[i][8] = imagebuffer[i][8] | BRIGHT_FLAG;
    }
}

static void replace_colour(uint8_t before, uint8_t after)
{
    uint8_t beforeink = before & 7;
    uint8_t afterink = after & 7;

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

static uint8_t ink2paper(uint8_t ink)
{
    uint8_t paper = (ink & INK_MASK) << 3; // 0000 0111 mask ink
    paper = paper & PAPER_MASK; // 0011 1000 mask paper
    return (ink & BRIGHT_FLAG) | paper; // 0x40 = 0100 0000 preserve brightness bit from ink
}

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

static void replace_paper_and_ink(uint8_t before, uint8_t after)
{
    uint8_t beforeink = before & (INK_MASK | BRIGHT_FLAG); // 0100 0111 ink + brightness
    replace(beforeink, after, INK_MASK | BRIGHT_FLAG);
    uint8_t beforepaper = ink2paper(before);
    uint8_t afterpaper = ink2paper(after);
    replace(beforepaper, afterpaper, PAPER_MASK | BRIGHT_FLAG); // 0111 1000 paper + brightness
}

void ClearGraphMem(void)
{
    memset(imagebuffer, 0, IRMAK_IMGSIZE * 9);
}

void DrawTaylor(int loc)
{
    uint8_t *ptr = taylor_image_data;
    for (int i = 0; i < loc; i++) {
        while (*(ptr) != 0xff)
            ptr++;
        ptr++;
    }

    while (ptr < EndOfGraphicsData) {
        //        fprintf(stderr, "DrawTaylorRoomImage: Instruction %d: 0x%02x\n", instruction++, *ptr);
        switch (*ptr) {
        case 0xff:
            //                fprintf(stderr, "End of picture\n");
            return;
        case 0xfe:
            //                fprintf(stderr, "0xfe mirror_left_half\n");
            mirror_area(0, 0, IRMAK_IMGWIDTH, IRMAK_IMGHEIGHT);
            break;
        case 0xfd:
            //                fprintf(stderr, "0xfd Replace colour %x with %x\n", *(ptr + 1), *(ptr + 2));
            replace_colour(*(ptr + 1), *(ptr + 2));
            ptr += 2;
            break;
        case 0xfc: // Draw colour: x, y, attribute, length 7808
            if (Version != HEMAN_TYPE) {
                // fprintf(stderr, "0xfc (7808) Draw attribute %x at %d,%d length %d\n", *(ptr + 3), *(ptr + 1), *(ptr + 2), *(ptr + 4));
                draw_colour_old(*(ptr + 1), *(ptr + 2), *(ptr + 3), *(ptr + 4));
                ptr = ptr + 4;
            } else {
                // fprintf(stderr, "0xfc (7808) Draw attribute %x at %d,%d height %d width %d\n", *(ptr + 4), *(ptr + 2), *(ptr + 1), *(ptr + 3), *(ptr + 5));
                draw_colour(*(ptr + 4), *(ptr + 2), *(ptr + 1), *(ptr + 5), *(ptr + 3));
                ptr = ptr + 5;
            }
            break;
        case 0xfb: // Make all screen colours bright 713e
            // fprintf(stderr, "Make colours in picture area bright\n");
            make_light();
            break;
        case 0xfa: // Flip entire image horizontally 7646
            // fprintf(stderr, "0xfa Flip entire image horizontally\n");
            flip_image_horizontally();
            break;
        case 0xf9: //0xf9 Draw picture n recursively;
            // fprintf(stderr, "Draw Room Image %d recursively\n", *(ptr + 1));
            DrawTaylor(*(ptr + 1));
            ptr++;
            break;
        case 0xf8:
            // fprintf(stderr, "0xf8: Skip rest of picture if object %d is not present\n", *(ptr + 1));
            ptr++;
            if (CurrentGame == BLIZZARD_PASS || BaseGame == REBEL_PLANET) {
                if (ObjectLoc[*ptr] == MyLoc) {
                    DrawSagaPictureAtPos(*(ptr + 1), *(ptr + 2), *(ptr + 3), 1);
                }
                ptr += 3;
            } else {
                if (ObjectLoc[*ptr] != MyLoc)
                    return;
            }
            break;
        case 0xf4: // End if object arg1 is present
            // fprintf(stderr, "0xf4: Skip rest of picture if object %d IS present\n", *(ptr + 1));
            if (ObjectLoc[*(ptr + 1)] == MyLoc)
                return;
            ptr++;
            break;
        case 0xf3:
            // fprintf(stderr, "0xf3: goto 753d Mirror top half vertically\n");
            mirror_top_half();
            break;
        case 0xf2: // arg1 arg2 arg3 arg4 Mirror horizontally
            // fprintf(stderr, "0xf2: Mirror area x: %d y: %d width:%d y2:%d horizontally\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
            mirror_area(*(ptr + 2), *(ptr + 1), *(ptr + 4), *(ptr + 3));
            ptr = ptr + 4;
            break;
        case 0xf1: // arg1 arg2 arg3 arg4 Mirror vertically
            // fprintf(stderr, "0xf1: Mirror area x: %d y: %d width:%d y2:%d vertically\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
            mirror_area_vertically(*(ptr + 1), *(ptr + 2), *(ptr + 4), *(ptr + 3));
            ptr = ptr + 4;
            break;
        case 0xee: // arg1 arg2 arg3 arg4  Flip area horizontally
            // fprintf(stderr, "0xf1: Flip area x: %d y: %d width:%d y2:%d horizontally\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
            flip_area_horizontally(*(ptr + 2), *(ptr + 1), *(ptr + 4), *(ptr + 3));
            ptr = ptr + 4;
            break;
        case 0xed:
            // fprintf(stderr, "0xed: Flip entire image vertically\n");
            flip_image_vertically();
            break;
        case 0xec: // Flip area vertically
            // fprintf(stderr, "0xf1: Flip area x: %d y: %d width:%d y2:%d vertically\n", *(ptr + 2), *(ptr + 1), *(ptr + 4),  *(ptr + 3));
            flip_area_vertically(*(ptr + 1), *(ptr + 2), *(ptr + 4), *(ptr + 3));
            ptr = ptr + 4;
            break;
        case 0xeb:
        case 0xea:
                fprintf(stderr, "Unimplemented draw instruction 0x%02x!\n", *ptr);
                break;
        case 0xe9:
            // fprintf(stderr, "0xe9: (77ac) replace paper and ink %d for colour %d?\n",  *(ptr + 1), *(ptr + 2));
            replace_paper_and_ink(*(ptr + 1), *(ptr + 2));
            ptr = ptr + 2;
            break;
        case 0xe8:
            // fprintf(stderr, "Clear graphics memory\n");
            ClearGraphMem();
            break;
        case 0xf7: // set A to 0c and call draw image, but A seems to not be used. Vestigial code?

          /* In the basement of the Zodiac Hotel in Rebel Planet, there is a locked alcove.
             There is an image of the bottles inside, but in the original interpreter it is never
             drawn.

             The image draw instructions for room 43 (the basement) calls opcode 0xf7 with the
             bottles image index as its first argument, but the original code just resturns without
             ever drawing it.

             The code below is a hack which checks if the phonic fork (object 131) is out of play
             (in dummy room 252), which only seems to be the case when the alcove is locked.
             If not, it falls through to the draw block instruction beneath, which will draw the
             bottles.

             This would potentially cause problems if not for the fact that instruction 0xf7 is
             used by no other image and by no other TaylorMade game.
             (No game seems to use 0xf6 or 0xf5, for that matter.) */
            if (BaseGame == REBEL_PLANET && MyLoc == 43 && ObjectLoc[131] == 252)
                return;
        case 0xf6: // set A to 04 and call draw image. See 0xf7 above.
        case 0xf5: // set A to 08 and call draw image. See 0xf7 above.
            // fprintf(stderr, "0x%02x: set A to unused value and draw image block %d at %d, %d\n",  *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3));
            ptr++; // Deliberate fallthrough
        default: // else draw image *ptr at x, y
            // fprintf(stderr, "Default: Draw image block %d at %d,%d\n", *ptr, *(ptr + 1), *(ptr + 2));
            DrawSagaPictureAtPos(*ptr, *(ptr + 1), *(ptr + 2), 1);
            ptr = ptr + 2;
            break;
        }
        ptr++;
    }
}

void DrawSagaPictureFromData(uint8_t *dataptr, int xsize, int ysize,
                             int xoff, int yoff, size_t datasize, int draw_to_buffer) {
    IrmakImgContext ctx;
    ctx.dataptr = dataptr;
    ctx.origptr = dataptr;
    ctx.xsize = xsize;
    ctx.ysize = ysize;
    ctx.xoff = xoff;
    ctx.yoff = yoff;
    ctx.version = 4;
    ctx.datasize = datasize;
    ctx.draw_to_buffer = draw_to_buffer;

    DrawIrmakPictureFromContext(ctx);
}

void DrawSagaPictureNumber(int picture_number)
{
    if (Game->number_of_pictures == 0)
        return;
    if (picture_number >= Game->number_of_pictures) {
        debug_print("Invalid image number %d! Last image:%d\n", picture_number,
                    Game->number_of_pictures - 1);
        return;
    }

    Image img = images[picture_number];

    if (img.imagedata == NULL)
        return;

    DrawSagaPictureFromData(img.imagedata, img.width, img.height, img.xoff,
        img.yoff, img.datasize, 1);
}

void DrawSagaPictureAtPos(int picture_number, int x, int y, int draw_to_buffer)
{
    Image img = images[picture_number];
    DrawSagaPictureFromData(img.imagedata, img.width, img.height, x, y, img.datasize, draw_to_buffer);
}

void PatchAndDrawQP3Cannon(void) {
    DrawSagaPictureNumber(46);
    imagebuffer[IRMAK_IMGWIDTH * 8 + 25][8] &= 191;
    imagebuffer[IRMAK_IMGWIDTH * 9 + 25][8] &= 191;
    imagebuffer[IRMAK_IMGWIDTH * 9 + 26][8] &= 191;
    imagebuffer[IRMAK_IMGWIDTH * 9 + 27][8] &= 191;
    DrawIrmakPictureFromBuffer();
}
