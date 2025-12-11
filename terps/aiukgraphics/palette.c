//
//  palette.c
//  scott
//
//  Created by Administrator on 2025-12-13.
//

#include "glk.h"
#include "palette.h"


int white_colour = 15;
int blue_colour = 9;
glui32 dice_colour = 0xff0000;

PALETTE pal;

palette_type palchosen = NO_PALETTE;

#define INVALIDCOLOR 16

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

        white_colour = 15;
        blue_colour = 9;
        dice_colour = 0xff0000;
    } else if (palchosen == ZXOPT) {
        /* optimized but not realistic Sinclair ZX palette (SPIN emu) */
        RGB black = { 0, 0, 0 };
        RGB blue = { 0, 0, 202 };
        RGB red = { 202, 0, 0 };
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
    } else {
        /* For all others we use this C64 palette (pepto/VICE) */
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

//static const char *colortext(int32_t col)
//{
//    const char *zxcolorname[] = {
//        "black",
//        "blue",
//        "red",
//        "magenta",
//        "green",
//        "cyan",
//        "yellow",
//        "white",
//        "bright black",
//        "bright blue",
//        "bright red",
//        "bright magenta",
//        "bright green",
//        "bright cyan",
//        "bright yellow",
//        "bright white",
//        "INVALID",
//    };
//
//    const char *c64colorname[] = {
//        "black",
//        "white",
//        "red",
//        "cyan",
//        "purple",
//        "green",
//        "blue",
//        "yellow",
//        "orange",
//        "brown",
//        "light red",
//        "dark grey",
//        "grey",
//        "light green",
//        "light blue",
//        "light grey",
//        "INVALID",
//    };
//
//    if (palchosen >= C64A)
//        return (c64colorname[col]);
//    else
//        return (zxcolorname[col]);
//}

uint8_t Remap(uint8_t color)
{
    int32_t mapcol;

    if ((palchosen == ZX) || (palchosen == ZXOPT)) {
        /* nothing to remap here; shows that the gfx were created on a ZX */
        mapcol = (color <= 15) ? color : INVALIDCOLOR;
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
        mapcol = (color <= 15) ? c64remap[color] : INVALIDCOLOR;
    } else if (palchosen == C64B) {
        /* remap B seem to be used in most C64 games after Mysterious Adventures
         and before Seas of Blood */
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
        mapcol = (color <= 15) ? c64remap[color] : INVALIDCOLOR;
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
        mapcol = (color <= 15) ? c64remap[color] : INVALIDCOLOR;
    } else if (palchosen == C64D) {
        /* remap D determined from Seas of Blood C64 */
        int32_t c64remap[] = {
            0,
            6,
            2,
            4,
            5,
            3,
            8, // (dark) yellow -> orange
            1,
            0,
            6,
            2,
            4,
            5,
            3,
            7, // bright yellow -> yellow
            1,
        };
        mapcol = (color <= 15) ? c64remap[color] : INVALIDCOLOR;
    } else if (palchosen == C64QP3) {
        /* remap used in Questprobe 3 C64 */
        /* This is the same as Seas Of Blood
         except non-bright yellow is mapped
         to yellow instead of orange */
        int32_t c64remap[] = {
            0, // black
            6, // blue
            2, // red
            4, // magenta
            5, // green
            3, // cyan
            7, // yellow -> yellow
            1, // white
            0, // bright black
            6, // bright blue
            2, // bright red
            4, // bright magenta
            5, // bright green
            3, // bright cyan
            7, // bright yellow -> yellow
            1, // bright white
        };
        mapcol = (color <= 15) ? c64remap[color] : INVALIDCOLOR;
    } else if (palchosen == C64TAYLOR) {
        /* remap B used in all other games */
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
        mapcol = (color <= 15) ? c64remap[color] : INVALIDCOLOR;
    } else
        mapcol = (color <= 15) ? color : INVALIDCOLOR;

    return (mapcol);
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
