//
//  Based on Sagadraw v2.5 by David Lodge
//  with help from Paul David Doherty (including the colour code)
//
//  Original code at https://github.com/tautology0/textadventuregraphics
//  See also http://aimemorial.if-legends.org/gfxbdp.html
//
//
//  The 8 x 8 pixel graphic building blocks used for these images were
//  previously referred to in code as "characters", but that was a bit
//  confusing, as they don't have anything to do with the characters
//  used to print text. Also, this kind of computer graphics is commonly
//  called "tile based" when used in arcade games and similar.
//
//  All instances of "character" are now changed to "tile".
//  Hopefully this will make things less rather than more confusing.
//


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "sagagraphics.h"
#include "scott.h"
#include "scottdefines.h"
#include "seasofblood.h"
#include "irmak.h"

#include "sagadraw.h"

Image *images = NULL;

static uint8_t *EndOfGraphicsData;

/* palette handler stuff starts here */

int white_colour = 15;
int blue_colour = 9;
glui32 dice_colour = 0xff0000;

int32_t errorcount = 0;

palette_type palchosen = NO_PALETTE;

#define INVALIDCOLOR 16

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

        white_colour = 15;
        blue_colour = 9;
        dice_colour = 0xff0000;
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

        white_colour = 15;
        blue_colour = 9;
        dice_colour = 0xff0000;
    } else if (palchosen >= C64A) {
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

        white_colour = 1;
        blue_colour = 6;
        dice_colour = 0x5f48e9;
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
            0,
            6,
            2,
            4,
            5,
            3,
            7,
            1,
            8,
            1,
            1,
            1,
            7,
            12,
            8,
            7,
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
            14,
            8,
            12,
            0,
            6,
            2,
            4,
            5,
            3,
            7,
            1,
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else if (palchosen == C64C) {
        /* remap C determined from Spiderman C64 */
        int32_t c64remap[] = {
            0,
            6,
            2,
            4,
            5,
            14,
            8,
            12,
            0,
            6,
            2,
            4,
            5,
            3,
            7,
            1,
        };
        mapcol = (((color >= 0) && (color <= 15)) ? c64remap[color] : INVALIDCOLOR);
    } else if (palchosen == C64D) {
        /* remap D determined from Seas of Blood C64 */
        int32_t c64remap[] = {
            0,
            6,
            2,
            4,
            5,
            3,
            8,
            1,
            0,
            6,
            2,
            4,
            5,
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

struct image_patch {
    GameIDType id;
    int picture_number;
    int offset;
    int number_of_bytes;
    const char *patch;
};

static const struct image_patch image_patches[] = {
    { UNKNOWN_GAME, 0, 0, 0, "" },
    { CLAYMORGUE_C64, 12, 378, 9, "\x20\xa4\x02\x80\x20\x82\x01\x20\x84" },
    { CLAYMORGUE_C64, 28, 584, 1, "\x90" },
    { CLAYMORGUE_C64, 16, 0, 12,
        "\x82\xfd\x00\x82\x81\x00\xff\x05\xff\x05\xff\x05" },
    { CLAYMORGUE, 14, 0, 12,
        "\x82\xfd\x00\x82\x81\x00\xff\x05\xff\x05\xff\x05" },
    { GREMLINS_C64, 21, 10, 5, "\x01\xa0\x03\x00\x01" },
    { GREMLINS_C64, 44, 304, 1, "\xb1" },
    { GREMLINS_C64, 81, 176, 1, "\xa0" },
    { GREMLINS_C64, 85, 413, 1, "\x94" },
    { GREMLINS_GERMAN_C64, 20, 191, 9, "\x03\x00\x94\x0c\xb0\x02\x94\x0d\xb0" },
    { SPIDERMAN_C64, 9, 1241, 1, "\x81" },
    { SECRET_MISSION_C64, 11, 88, 2, "\x90\x18" },
    { SECRET_MISSION, 33, 2, 1, "\xa0" },
    { SECRET_MISSION_C64, 33, 2, 1, "\xa0" },
    { SECRET_MISSION, 38, 53, 1, "\x53" },
    { SECRET_MISSION_C64, 38, 53, 1, "\x53" },
    { SEAS_OF_BLOOD_C64, 34, 471, 1, "\xa0" },
    { SEAS_OF_BLOOD_C64, 35, 16, 1, "\xcc" },
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

static void PatchOutBrokenClaymorgueImagesC64(void)
{
    Output("[This copy of The Sorcerer of Claymorgue Castle has 16 broken or "
           "missing pictures. These have been patched out.]\n\n");
    for (int i = 12; i < 28; i++) {
        if (i != 16)
            for (int j = 0; j < GameHeader.NumRooms; j++) {
                if (Rooms[j].Image == i) {
                    Rooms[j].Image = 255;
                }
            }
    }
}

static void PatchOutBrokenClaymorgueImagesZX(void)
{
    Output("[This copy of The Sorcerer of Claymorgue Castle has 26 broken or "
           "missing pictures. These have been patched out.]\n\n");
    for (int i = 9; i < 36; i++) {
        if (i != 14)
            for (int j = 0; j < GameHeader.NumRooms; j++) {
                if (Rooms[j].Image == i) {
                    Rooms[j].Image = 255;
                }
            }
    }
}

size_t hulk_coordinates = 0x26db;
size_t hulk_item_image_offsets = 0x2798;
size_t hulk_look_image_offsets = 0x27bc;
size_t hulk_special_image_offsets = 0x276e;
size_t hulk_image_offset = 0x441b;

void SagaSetup(size_t imgoffset)
{
	if (images != NULL)
        return;

    EndOfGraphicsData = entire_file + file_length;

    int32_t i, y;

    uint16_t image_offsets[Game->number_of_pictures];

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
    }

    if (palchosen == NO_PALETTE) {
        debug_print("unknown palette\n");
        exit(EXIT_FAILURE);
    }

    DefinePalette();

    int version = Game->picture_format_version;

    int32_t tiles_start = Game->start_of_tiles + file_baseline_offset;
    int32_t offset_table_start = Game->start_of_image_data + file_baseline_offset;

    if (Game->start_of_image_data == FOLLOWS) {
        offset_table_start = tiles_start + 0x800;
    }

    int32_t data_offset = Game->image_address_offset + file_baseline_offset;
    if (imgoffset)
        data_offset = imgoffset;
    uint8_t *pos;
    int numgraphics = Game->number_of_pictures;
    pos = SeekToPos(entire_file, tiles_start);

#ifdef DRAWDEBUG
    debug_print("Grabbing tile details\n");
    debug_print("Tile Offset: %04x\n", tiles_start - file_baseline_offset);
#endif
    for (i = 0; i < 256; i++) {
        for (y = 0; y < 8 && pos < EndOfGraphicsData; y++) {
            tiles[i][y] = *(pos++);
        }
    }

    /* Now we have hopefully read the tile data */
    /* Time for the image offsets */

    images = (Image *)MemAlloc(sizeof(Image) * numgraphics);
    Image *img = images;

    pos = SeekToPos(entire_file, offset_table_start);

    int broken_claymorgue_pictures_c64 = 0;
    int broken_claymorgue_pictures_zx = 0;

    for (i = 0; i < numgraphics; i++) {
        if (Game->picture_format_version == 0) {
            uint16_t address;

            if (i < 11) {
                address = Game->start_of_image_data + (i * 2);
            } else if (i < 28) {
                address = hulk_item_image_offsets + (i - 10) * 2;
            } else if (i < 34) {
                address = hulk_look_image_offsets + (i - 28) * 2;
            } else {
                address = hulk_special_image_offsets + (i - 34) * 2;
            }

            address += file_baseline_offset;
            address = entire_file[address] + entire_file[address + 1] * 0x100;

            image_offsets[i] = address + hulk_image_offset;
        } else {
            image_offsets[i] = *(pos++);
            image_offsets[i] += *(pos++) * 0x100;
        }
    }

    for (int picture_number = 0; picture_number < numgraphics; picture_number++) {
        pos = SeekToPos(entire_file, image_offsets[picture_number] + data_offset);
        if (pos == 0)
            return;

        img->width = *(pos++);
        if (img->width > 32)
            img->width = 32;

        img->height = *(pos++);
        if (img->height > 12)
            img->height = 12;

        if (version > 0) {
            img->xoff = *(pos++);
            if (img->xoff > 32)
                img->xoff = 4;
            img->yoff = *(pos++);
            if (img->yoff > 12)
                img->yoff = 0;
        } else {
            if (picture_number > 9 && picture_number < 28) {
                img->xoff = entire_file[hulk_coordinates + picture_number - 10 + file_baseline_offset];
                img->yoff = entire_file[hulk_coordinates + 18 + picture_number - 10 + file_baseline_offset];
            } else {
                img->xoff = img->yoff = 0;
            }
        }

        if (CurrentGame == CLAYMORGUE_C64 && img->height == 0 && img->width == 0 && picture_number == 13) {
            PatchOutBrokenClaymorgueImagesC64();
            broken_claymorgue_pictures_c64 = 1;
        }

        if (broken_claymorgue_pictures_c64 && (picture_number == 16 || picture_number == 28)) {
            img->height = 12;
            img->width = 32;
            img->xoff = 4;
            img->yoff = 0;
            if (picture_number == 28)
                pos = entire_file + 0x2005;
        }

        if (CurrentGame == CLAYMORGUE && img->height == 0 && img->width == 0 && picture_number == 9) {
            PatchOutBrokenClaymorgueImagesZX();
            broken_claymorgue_pictures_zx = 1;
        }

        if (broken_claymorgue_pictures_zx && (picture_number == 14)) {
            img->height = 12;
            img->width = 32;
            img->xoff = 4;
            img->yoff = 0;
        }

        img->imagedata = pos;
        img->datasize = EndOfGraphicsData - pos;

        int patch = FindImagePatch(CurrentGame, picture_number, 0);
        while (patch) {
            Patch(img->imagedata, patch);
            patch = FindImagePatch(CurrentGame, picture_number, patch);
        }

        img++;
    }
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

void SwitchPalettes(int pal1, int pal2)
{
    uint8_t temp[3];

    temp[0] = pal[pal1][0];
    temp[1] = pal[pal1][1];
    temp[2] = pal[pal1][2];

    pal[pal1][0] = pal[pal2][0];
    pal[pal1][1] = pal[pal2][1];
    pal[pal1][2] = pal[pal2][2];

    pal[pal2][0] = temp[0];
    pal[pal2][1] = temp[1];
    pal[pal2][2] = temp[2];
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
    ctx.version = Game->picture_format_version;
    ctx.datasize = datasize;
    ctx.offsetlimit = xsize * ysize;
    ctx.draw_to_buffer = draw_to_buffer;

    DrawIrmakPictureFromContext(ctx);
}

void DrawSagaPictureNumber(int picture_number, int draw_to_buffer)
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

    DrawSagaPictureFromData(img.imagedata, img.width, img.height,
                            img.xoff, img.yoff, img.datasize, draw_to_buffer);

    last_image_index = picture_number;
}

void DrawSagaPictureAtPos(int picture_number, int x, int y, int draw_to_buffer)
{
    Image img = images[picture_number];
    DrawSagaPictureFromData(img.imagedata, img.width, img.height, x, y, img.datasize, draw_to_buffer);
}
