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

static void SetColor(int32_t index, RGB colour)
{
    pal[index] = colour;
}

void DefinePalette(void)
{
    /* set up the palette */
    if (palchosen == VGA) {
        RGB black =     0x000000;
        RGB blue =      0x0000ff;
        RGB red =       0xff0000;
        RGB magenta =   0xff00ff;
        RGB green =     0x00ff00;
        RGB cyan =      0x00ffff;
        RGB yellow =    0xffff00;
        RGB white =     0xffffff;
        RGB brblack =   0x000000;
        RGB brblue =    0x0000ff;
        RGB brred =     0xff0000;
        RGB brmagenta = 0xff00ff;
        RGB brgreen =   0x00ff00;
        RGB brcyan =    0x00ffff;
        RGB bryellow =  0xffff00;
        RGB brwhite =   0xffffff;

        SetColor(0, black);
        SetColor(1, blue);
        SetColor(2, red);
        SetColor(3, magenta);
        SetColor(4, green);
        SetColor(5, cyan);
        SetColor(6, yellow);
        SetColor(7, white);
        SetColor(8, brblack);
        SetColor(9, brblue);
        SetColor(10, brred);
        SetColor(11, brmagenta);
        SetColor(12, brgreen);
        SetColor(13, brcyan);
        SetColor(14, bryellow);
        SetColor(15, brwhite);
    } else if (palchosen == ZX) {
        /* corrected Sinclair ZX palette (pretty dull though) */
        RGB black =     0x000000;
        RGB blue =      0x00009a;
        RGB red =       0x9a0000;
        RGB magenta =   0x9a009a;
        RGB green =     0x009a00;
        RGB cyan =      0x009a9a;
        RGB yellow =    0x9a9a00;
        RGB white =     0x9a9a9a;
        RGB brblack =   0x000000;
        RGB brblue =    0x0000aa;
        RGB brred =     0xba0000;
        RGB brmagenta = 0xce00ce;
        RGB brgreen =   0x00ce00;
        RGB brcyan =    0x00dfdf;
        RGB bryellow =  0xefef00;
        RGB brwhite =   0xffffff;

        SetColor(0, black);
        SetColor(1, blue);
        SetColor(2, red);
        SetColor(3, magenta);
        SetColor(4, green);
        SetColor(5, cyan);
        SetColor(6, yellow);
        SetColor(7, white);
        SetColor(8, brblack);
        SetColor(9, brblue);
        SetColor(10, brred);
        SetColor(11, brmagenta);
        SetColor(12, brgreen);
        SetColor(13, brcyan);
        SetColor(14, bryellow);
        SetColor(15, brwhite);

        white_colour = 15;
        blue_colour = 9;
        dice_colour = 0xff0000;
    } else if (palchosen == ZXOPT) {
        /* optimized but not realistic Sinclair ZX palette (SPIN emu) */
        RGB black =   0x000000;
        RGB blue =    0x0000ca;
        RGB red =     0xca0000;
        RGB magenta = 0xca00ca;
        RGB green =   0x00ca00;
        RGB cyan =    0x00caca;
        RGB yellow =  0xcaca00;
        RGB white =   0xcacaca;
        /*
         old David Lodge palette:

         RGB black =   0x000000;
         RGB blue =    0x0000d6;
         RGB red =     0xd60000;
         RGB magenta = 0xd600d6;
         RGB green =   0x00d600;
         RGB cyan =    0x00d6d6;
         RGB yellow =  0xd6d600;
         RGB white =   0xd6d6d6;
         */
        RGB brblack =   0x000000;
        RGB brblue =    0x0000ff;
        RGB brred =     0xff0014;
        RGB brmagenta = 0xff00ff;
        RGB brgreen =   0x00ff00;
        RGB brcyan =    0x00ffff;
        RGB bryellow =  0xffff00;
        RGB brwhite =   0xffffff;

        SetColor(0, black);
        SetColor(1, blue);
        SetColor(2, red);
        SetColor(3, magenta);
        SetColor(4, green);
        SetColor(5, cyan);
        SetColor(6, yellow);
        SetColor(7, white);
        SetColor(8, brblack);
        SetColor(9, brblue);
        SetColor(10, brred);
        SetColor(11, brmagenta);
        SetColor(12, brgreen);
        SetColor(13, brcyan);
        SetColor(14, bryellow);
        SetColor(15, brwhite);

        white_colour = 15;
        blue_colour = 9;
        dice_colour = 0xff0000;
    } else {
        /* For all others we use this C64 palette (pepto/VICE) */
        RGB black =  0x000000;
        RGB white =  0xffffff;
        RGB red =    0xbf6148;
        RGB cyan =   0x99e6f9;
        RGB purple = 0xb159b9;
        RGB green =  0x79d570;
        RGB blue =   0x5f48e9;
        RGB yellow = 0xf7ff6c;
        RGB orange = 0xba8620;
        RGB brown =  0x746900;
        RGB lred =   0xe79a84;
        RGB dgrey =  0x454545;
        RGB grey =   0xa7a7a7;
        RGB lgreen = 0xc0ffb9;
        RGB lblue =  0xa28fff;
        RGB lgrey =  0xc8c8c8;

        SetColor(0, black);
        SetColor(1, white);
        SetColor(2, red);
        SetColor(3, cyan);
        SetColor(4, purple);
        SetColor(5, green);
        SetColor(6, blue);
        SetColor(7, yellow);
        SetColor(8, orange);
        SetColor(9, brown);
        SetColor(10, lred);
        SetColor(11, dgrey);
        SetColor(12, grey);
        SetColor(13, lgreen);
        SetColor(14, lblue);
        SetColor(15, lgrey);

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
    glui32 temp;

    temp = pal[pal1];
    pal[pal1] = pal[pal2];
    pal[pal2] = temp;
}
