//
//  palette.c
//  scott
//
//  Created by Administrator on 2025-12-13.
//
//  Colour palette definitions and remapping for
//  Adventure International UK / Adventure Soft tile graphics.
//
//  The original games ran on two very different platforms — the ZX
//  Spectrum (8 colours × 2 brightness levels = 16 entries) and the
//  Commodore 64 (16 fixed colours, completely different ordering).
//  The image data always uses the Spectrum colour numbering (0–7
//  normal, 8–15 bright). On C64, a remap table translates those
//  indices into the closest C64 equivalents.
//
//  Several C64 remap variants exist because different game releases
//  chose slightly different colour substitutions.
//

#include "glk.h"
#include "palette.h"


/* Platform-dependent colour indices used by game-specific drawing code.
   These are palette indices (into pal[]), not RGB values, and are set
   by DefinePalette() to match the active platform's colour ordering. */
int white_colour = 15;
int blue_colour = 9;
glui32 dice_colour = 0xff0000;  /* RGB, used by Seas of Blood */

/* The active 16-entry palette: pal[i] holds an 0xRRGGBB value. */
glui32 pal[16];

palette_type palchosen = NO_PALETTE;

/* Sentinel value returned by Remap() for out-of-range colour indices. */
#define INVALIDCOLOR 16

static void SetColor(int32_t index, glui32 colour)
{
    pal[index] = colour;
}

/* Populate the 16-entry pal[] array with RGB values for the chosen
   platform palette, and set the platform-dependent colour indices
   (white_colour, blue_colour, dice_colour).

   Palette index layout for ZX/VGA modes:
     0–7:  normal colours (black, blue, red, magenta, green, cyan, yellow, white)
     8–15: bright variants of the same colours

   For C64 modes the 16 entries follow the C64's native colour order
   (black, white, red, cyan, purple, green, blue, yellow, orange,
   brown, light red, dark grey, grey, light green, light blue, light grey). */
void DefinePalette(void)
{
    if (palchosen == VGA) {
        glui32 black =     0x000000;
        glui32 blue =      0x0000ff;
        glui32 red =       0xff0000;
        glui32 magenta =   0xff00ff;
        glui32 green =     0x00ff00;
        glui32 cyan =      0x00ffff;
        glui32 yellow =    0xffff00;
        glui32 white =     0xffffff;
        glui32 brblack =   0x000000;
        glui32 brblue =    0x0000ff;
        glui32 brred =     0xff0000;
        glui32 brmagenta = 0xff00ff;
        glui32 brgreen =   0x00ff00;
        glui32 brcyan =    0x00ffff;
        glui32 bryellow =  0xffff00;
        glui32 brwhite =   0xffffff;

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
        /* Authentic Sinclair ZX Spectrum palette. The non-bright colours
           are noticeably dim (0x9A ≈ 60% intensity) compared to the
           bright variants — this matches real hardware output. */
        glui32 black =     0x000000;
        glui32 blue =      0x00009a;
        glui32 red =       0x9a0000;
        glui32 magenta =   0x9a009a;
        glui32 green =     0x009a00;
        glui32 cyan =      0x009a9a;
        glui32 yellow =    0x9a9a00;
        glui32 white =     0x9a9a9a;
        glui32 brblack =   0x000000;
        glui32 brblue =    0x0000aa;
        glui32 brred =     0xba0000;
        glui32 brmagenta = 0xce00ce;
        glui32 brgreen =   0x00ce00;
        glui32 brcyan =    0x00dfdf;
        glui32 bryellow =  0xefef00;
        glui32 brwhite =   0xffffff;

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
        /* Brighter ZX palette matching the SPIN emulator's output.
           Less authentic than ZX but more visually appealing on
           modern displays. */
        glui32 black =   0x000000;
        glui32 blue =    0x0000ca;
        glui32 red =     0xca0000;
        glui32 magenta = 0xca00ca;
        glui32 green =   0x00ca00;
        glui32 cyan =    0x00caca;
        glui32 yellow =  0xcaca00;
        glui32 white =   0xcacaca;
        /*
         old David Lodge palette:

         glui32 black =   0x000000;
         glui32 blue =    0x0000d6;
         glui32 red =     0xd60000;
         glui32 magenta = 0xd600d6;
         glui32 green =   0x00d600;
         glui32 cyan =    0x00d6d6;
         glui32 yellow =  0xd6d600;
         glui32 white =   0xd6d6d6;
         */
        glui32 brblack =   0x000000;
        glui32 brblue =    0x0000ff;
        glui32 brred =     0xff0014;
        glui32 brmagenta = 0xff00ff;
        glui32 brgreen =   0x00ff00;
        glui32 brcyan =    0x00ffff;
        glui32 bryellow =  0xffff00;
        glui32 brwhite =   0xffffff;

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
        /* Commodore 64 palette based on pepto's analysis (used by the
           VICE emulator). The C64 has a completely different colour
           ordering from the Spectrum, so the Remap() function
           translates Spectrum colour indices into C64 palette entries. */
        glui32 black =  0x000000;
        glui32 white =  0xffffff;
        glui32 red =    0xbf6148;
        glui32 cyan =   0x99e6f9;
        glui32 purple = 0xb159b9;
        glui32 green =  0x79d570;
        glui32 blue =   0x5f48e9;
        glui32 yellow = 0xf7ff6c;
        glui32 orange = 0xba8620;
        glui32 brown =  0x746900;
        glui32 lred =   0xe79a84;
        glui32 dgrey =  0x454545;
        glui32 grey =   0xa7a7a7;
        glui32 lgreen = 0xc0ffb9;
        glui32 lblue =  0xa28fff;
        glui32 lgrey =  0xc8c8c8;

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

/* Translate a Spectrum-format colour index (0–15) into the
   corresponding palette index for the active platform.

   For ZX/ZXOPT palettes the mapping is identity — the image data
   already uses Spectrum numbering. For C64 palettes, a remap table
   converts each Spectrum colour to its closest C64 equivalent.
   Different game releases used slightly different mappings, hence
   the multiple C64 variants (C64A through C64TAYLOR). */
uint8_t Remap(uint8_t color)
{
    int32_t mapcol;

    if ((palchosen == ZX) || (palchosen == ZXOPT)) {
        /* Identity mapping: image data is already in Spectrum order */
        mapcol = (color <= 15) ? color : INVALIDCOLOR;
    } else if (palchosen == C64A) {
        /* Earliest C64 remap, determined from The Golden Baton.
           Also used by other early Mysterious Adventures (S1/S3/S13).
           These games only used 8 colours. */
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
        /* Used by most C64 games released between the Mysterious
           Adventures and Seas of Blood (e.g. Gremlins, Secret Mission).
           The non-bright half maps differently from C64A. */
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
        /* Determined from Spider-Man C64. Differs from C64B only in
           the non-bright cyan slot (14 instead of 12). */
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
        /* Determined from Seas of Blood C64. Notable difference:
           non-bright yellow maps to orange (8) while bright yellow
           maps to yellow (7). */
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
        /* Questprobe 3 (Fantastic Four) C64. Same as C64D (Seas of
           Blood) except non-bright yellow maps to yellow (7) instead
           of orange (8). */
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
        /* Used by TaylorMade C64 games (He-Man, Blizzard Pass,
           Rebel Planet). The bright half maps differently from
           the non-bright half, unlike C64D/C64QP3 which mirror. */
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


/* Swap two palette entries. Only used by the Robin
   of Sherwood lightning flash animation. */
void SwitchPalettes(int pal1, int pal2)
{
    glui32 temp;

    temp = pal[pal1];
    pal[pal1] = pal[pal2];
    pal[pal2] = temp;
}
